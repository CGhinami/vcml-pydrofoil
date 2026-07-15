/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include <chrono>
#include <iostream>
#include <ostream>
#include <string>

// The timer starts on construction and prints on destruction
class Profiler {
    public:
    std::string fname;
    std::chrono::high_resolution_clock::time_point start;

    Profiler(std::string fname): fname(fname), start(std::chrono::high_resolution_clock::now()) {}

    virtual ~Profiler()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << fname << ": " << duration.count() << std::endl;
    }
};
