/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#pragma once
#include <atomic>
#include <chrono>
#include <thread>

namespace backend {

// A short busy-wait before falling back to std::this_thread::yield().
// Avoids ever calling into the kernel (mutex/condition_variable) on the fast
// path, which is what makes std::condition_variable both slow and noisy.
// A long wait (e.g. for a whole quantum of guest code) ends up sleeping: every
// hart has two threads of which one is always waiting, and with more busy
// threads than host CPUs the ones doing the actual work would otherwise only
// get a CPU every scheduler time slice.
inline void relax(int& spins)
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#endif
    ++spins;
    if(spins > 8000)
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    else if(spins > 4000)
        std::this_thread::yield();
}

// This is NOT a general-purpose queue: it only supports one
// request at a time, which is exactly the request/response
// pattern PythonTask and MemAccess use (the producer always blocks until the
// consumer answers before submitting the next one). That restriction is what
// lets it be lock-free: no mutex, no condition_variable, no syscalls on the
// fast path, just an atomic state flag.
template<typename T>
class Mailbox {
public:
    // Producer, step 1: publish the request. Non-blocking 
    void post(T& payload)
    {
        m_payload = &payload;
        m_state.store(State::REQUEST, std::memory_order_release);
    }

    // Producer, step 2: block (spin) until the consumer calls complete(),
    // then reset the mailbox so it can be reused for the next request.
    void wait_done()
    {
        int spins = 0;
        while(m_state.load(std::memory_order_acquire) != State::DONE)
            relax(spins);
        m_state.store(State::IDLE, std::memory_order_relaxed);
    }

    // Producer, non-blocking check for step 2
    bool is_done() const { return m_state.load(std::memory_order_acquire) == State::DONE; }

    // Convenience for the common case where the producer has nothing else to
    // do while waiting.
    void submit(T& payload)
    {
        post(payload);
        wait_done();
    }

    // Consumer: block (spin) until a request is published, must be followed by complete() once handled.
    T& take()
    {
        int spins = 0;
        while(m_state.load(std::memory_order_acquire) != State::REQUEST)
            relax(spins);
        return *m_payload;
    }

    // Consumer, non-blocking poll for a request: used when the consumer
    // side also needs to watch for something else concurrently.
    T* try_take()
    {
        return m_state.load(std::memory_order_acquire) == State::REQUEST ? m_payload : nullptr;
    }

    // Consumer: mark the request handed back by take()/try_take() as done.
    // Unblocks the producer's wait_done()/is_done().
    void complete() { m_state.store(State::DONE, std::memory_order_release); }

private:
    enum class State { IDLE, REQUEST, DONE };

    std::atomic<State> m_state{State::IDLE};
    T* m_payload = nullptr;
};

} // namespace backend
