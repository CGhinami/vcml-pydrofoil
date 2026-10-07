from _pydrofoilcapi_cffi import ffi
import _pydrofoil
from time import perf_counter

import os
import sys
sys.modules['__main__'] = type(sys)('__main__')

# PYDROFOIL_JIT=<opts>: the equivalent of the standalone's --jit flag
_jitopts = os.environ.get('PYDROFOIL_JIT')
if _jitopts:
    try:
        import pypyjit
        pypyjit.set_param(_jitopts)
        print("JIT: set_param(%r)" % (_jitopts,))
    except Exception as _e:
        print("JIT: set_param(%r) failed: %s" % (_jitopts, _e))

all_cpu_handles = []

class C:
    def __init__(self, rv64, n=None):
        self.rv64 = rv64
        self.arg = n
        self.callbacks = None
        self.breakpoints = []  # list of breakpoints
        self.verbosity = True
        self.max_over = 0      # worst overshoot past a simulate() request
        # reset() builds a brand new machine object -- and with it a new
        # globals object -- so anything we configure on the model has to be
        # remembered here and re-applied there, not just set once.
        self.htif_tohost = None
        self.ext_clint = None
        self.irq_lines_ptr = None
        self.insns_per_tick = None
        self.hartid = None
        self.reset()

    def _set_callbacks(self, read, write, payload):
        self.read = read
        self.write = write
        self.mem = ffi.new('uint64_t[1]')
        
        WIDTH_MAP = {
            8: ('uint64_t*', 64),
            4: ('uint32_t*', 32),
            2: ('uint16_t*', 16),
            1: ('uint8_t*', 8),
        }

        def resolve_width(width):
            try:
                return WIDTH_MAP[width]
            except KeyError:
                raise ValueError(f"Unsupported width: {width}")

        def pyread(addr, width):
            addr = int(addr)
            ptr_type, bitv_size = resolve_width(width)

            # Fall back to callback
            res = self.read(self._handle, addr, width, ffi.cast(ptr_type, self.mem), payload)
            assert res == 0
            return _pydrofoil.bitvector(bitv_size, self.mem[0])
        
        def pywrite(addr, width, value):
            addr = int(addr)
            value = int(value)
            ptr_type, bitv_size = resolve_width(width)

            res = self.write(self._handle, addr, width, value, payload)
            assert res == 0
        self.callbacks = _pydrofoil.Callbacks(mem_read_intercept=pyread, mem_write_intercept=pywrite)

    def set_verbosity(self, verbosity):
        self.verbosity = verbosity
        self.cpu.set_verbosity(verbosity)

    def step(self):
        self.steps += 1
        self.cpu.step()

    def reset(self):
        if self.rv64:
            cls = _pydrofoil.RISCV64
        else:
            cls = _pydrofoil.RISCV32
        if self.callbacks:
            self.cpu = cls(self.arg, callbacks=self.callbacks)
        else:
            self.cpu = cls(self.arg)
        self.steps = 0
        self.cpu._set_sail_memory_bounds(0x00000000, 0x4000000000)
        if self.htif_tohost is not None:
            self.cpu._set_htif_tohost(self.htif_tohost)
        if self.ext_clint is not None:
            self.cpu._set_ext_clint(self.ext_clint)
        if self.insns_per_tick is not None:
            self.cpu._set_instructions_per_tick(self.insns_per_tick)
        if self.irq_lines_ptr is not None:
            self.cpu._set_irq_lines_ptr(self.irq_lines_ptr)
        if self.hartid is not None:
            self.cpu.write_register('mhartid', self.hartid)
        self.set_verbosity(self.verbosity)

def _apply_jit_params():
    """PYDROFOIL_JIT=<pypyjit set_param string>, e.g. "off" or
    "threshold=100000".
    """
    import os
    params = os.environ.get("PYDROFOIL_JIT")
    if not params:
        return
    try:
        import pypyjit
        pypyjit.set_param(params)
        print("JIT: set_param(%r)" % params)
    except Exception as e:
        print("JIT: set_param(%r) failed: %s" % (params, e))


@ffi.def_extern()
def pydrofoil_allocate_cpu(spec, fn):
    _apply_jit_params()
    if spec:
        rv64 = "64" in ffi.string(spec).decode('utf-8')
    else:
        rv64 = True
    if fn:
        filename = ffi.string(fn).decode('utf-8')
    else:
        filename = None
    print("rv64" if rv64 else "rv32")
    print(filename)

    all_cpu_handles.append(res := ffi.new_handle(cpu := C(rv64, filename)))
    cpu._handle = res
    return res

@ffi.def_extern()
def pydrofoil_free_cpu(i):
    # Report anything gathered over the whole run
    try:
        cpu = ffi.from_handle(i)
        cpu.cpu.print_pc_hist()
        prog, calls, insns, bails, dyn = cpu.cpu.aot_stats()
        print("AOT stats: programs=0x%x dispatches=%d insns_in_blocks=%d "
              "bails=%d dyn_iters=%d" % (prog, calls, insns, bails, dyn))
        # Split of the bails by cause. A high bail rate is only actionable
        # once you know which guard is refusing: irq and tier are settled
        # before the context marshal, mmu is the Sv39 code-page walk,
        # guard is a block's own footprint test.
        try:
            irq, tier, mmu, grd = cpu.cpu.aot_bail_stats()
            print("AOT bails: irq=%d tier=%d mmu=%d guard=%d" %
                  (irq, tier, mmu, grd))
        except AttributeError:
            pass
        # Escapes: instructions a block could not express, run through the
        # model mid-block.
        try:
            esc, stops = cpu.cpu.aot_escape_stats()
            print("AOT escapes: %d (stopped the block: %d)" % (esc, stops))
        except AttributeError:
            pass
        # The dispatch budget
        # try:
        #     bstops, dover, rover = cpu.cpu.aot_budget_stats()
        #     print("AOT budget: stopped %d dispatches, max overshoot %d insns, "
        #           "max past a quantum %d insns" % (bstops, dover, rover))
        # except AttributeError:
        #     pass
        # print("VP quantum: max instructions past a simulate() request: %d"
        #       % cpu.max_over)
    except Exception as e:
        print("AOT teardown report failed:", e)

    try:
        all_cpu_handles.remove(i)
    except Exception:
        return -1
    return 0

@ffi.def_extern()
def pydrofoil_cpu_set_pc(i, value):
    cpu = ffi.from_handle(i)
    cpu.cpu.write_register('pc', value)
    cpu.reset()
    return 0


@ffi.def_extern()
def pydrofoil_cpu_pc(i):
    cpu = ffi.from_handle(i)
    return cpu.cpu.read_register('pc')

@ffi.def_extern()
def pydrofoil_cpu_set_breakpoint(i, addr):
    cpu = ffi.from_handle(i)
    
    if addr in cpu.breakpoints:
        return 0
    
    cpu.breakpoints.append(addr)
    return 0

@ffi.def_extern()
def pydrofoil_cpu_remove_breakpoint(i, addr):
    cpu = ffi.from_handle(i)

    try:
        cpu.breakpoints.remove(addr)
        return 0
    except ValueError:
        return 1


@ffi.def_extern()
def pydrofoil_cpu_set_ram_read_write_callback(i, read_cb, write_cb, payload):
    cpu = ffi.from_handle(i)
    cpu._set_callbacks(read_cb, write_cb, payload)
    cpu.reset()
    return 0

@ffi.def_extern()
def pydrofoil_cpu_simulate(i, steps):
    cpu = ffi.from_handle(i)
    cpu.steps = 0

    #start = perf_counter()

    if not cpu.breakpoints:
        # Track the worst overshoot past the quantum as the platform
        # sees it (instructions returned minus instructions asked), so
        # it can be reported at teardown for any plugin build.
        n = cpu.cpu.run(steps)
        over = n - steps
        if over > cpu.max_over:
            cpu.max_over = over
        return n

    # end = perf_counter()
    # elapsed = end-start
    # print("Steps: " + str(steps) + " Time needed: " + str(elapsed))     

    for _ in range(steps):
        
        pc_val = cpu.cpu.read_register('pc')

        if pc_val in cpu.breakpoints: # Check if the pc is in the list
            return cpu.steps # return if it is

        cpu.step()

    return cpu.steps

@ffi.def_extern()
def pydrofoil_cpu_cycles(i):
    cpu = ffi.from_handle(i)
    return cpu.steps

@ffi.def_extern()
def pydrofoil_cpu_read_reg(i, name):
    cpu = ffi.from_handle(i)
    try:
        reg_name = ffi.string(name).decode('utf-8')
        return cpu.cpu.read_register(reg_name)
    except ValueError:
        print("Register " + reg_name + " not found")
        return 1

# @ffi.def_extern()
# def pydrofoil_set_interrupt_pending(i, bit, set):
#     cpu = ffi.from_handle(i)

#     bit_size = 64 if cpu.rv64 else 32

#     mask = _pydrofoil.bitvector(bit_size, 1) << bit
#     mip = cpu.cpu.read_register('mip')
#     if set:
#         mip = mip | mask
#     else:
#         mip = mip & ~mask
#     cpu.cpu.write_register('mip', mip)
#     return 0


@ffi.def_extern()
def pydrofoil_cpu_set_htif_tohost(i, tohost):
    cpu = ffi.from_handle(i)
    cpu.htif_tohost = int(tohost)
    cpu.cpu._set_htif_tohost(cpu.htif_tohost)
    return 0

@ffi.def_extern()
def pydrofoil_cpu_set_external_clint(i, enable):
    cpu = ffi.from_handle(i)
    cpu.ext_clint = int(enable)
    cpu.cpu._set_ext_clint(cpu.ext_clint)
    return 0

@ffi.def_extern()
def pydrofoil_set_interrupt_lines(i, lines_ptr):
    cpu = ffi.from_handle(i)
    cpu.irq_lines_ptr = int(ffi.cast("uintptr_t", lines_ptr))
    cpu.cpu._set_irq_lines_ptr(cpu.irq_lines_ptr)
    return 0

@ffi.def_extern()
def pydrofoil_set_instructions_per_tick(i, insns_per_tick):
    cpu = ffi.from_handle(i)
    cpu.insns_per_tick = int(insns_per_tick)
    cpu.cpu._set_instructions_per_tick(cpu.insns_per_tick)
    return 0

@ffi.def_extern()
def pydrofoil_cpu_set_hartid(i, hartid):
    cpu = ffi.from_handle(i)
    cpu.hartid = int(hartid)
    try:
        cpu.cpu.write_register('mhartid', cpu.hartid)
    except Exception as e:
        print("Setting mhartid failed:", e)
        return 1
    return 0

@ffi.def_extern()
def pydrofoil_cpu_reset(i):
    cpu = ffi.from_handle(i)
    cpu.reset()
    return 0

@ffi.def_extern()
def pydrofoil_cpu_set_verbosity(i, v):
    cpu = ffi.from_handle(i)
    cpu.set_verbosity(bool(v))
    return 0

@ffi.def_extern()
def pydrofoil_cpu_write_reg(i, name, val):
    cpu = ffi.from_handle(i)
    reg_name = ffi.string(name).decode('utf-8')
    try:
        cpu.cpu.write_register(reg_name, val)
        return 0
    except ValueError:
        print("Register " + reg_name + " not found")
        return 1

@ffi.def_extern()
def pydrofoil_cpu_set_dma_region(i, base_address, size, memory):
    cpu = ffi.from_handle(i)
    if cpu.callbacks is None:
        return -1  # RAM callbacks must be set first
    
    ptr_val = int(ffi.cast("uintptr_t", memory))
    cpu.cpu.add_dmi_region(base_address, size, ptr_val)
    return 0

@ffi.def_extern()
def pydrofoil_get_htif_done(i):
    cpu = ffi.from_handle(i)
    return cpu.cpu.get_htif_done()

sys.modules['__main__'].__dict__.update(globals())
sys.argv = ['embedded-pypy']
