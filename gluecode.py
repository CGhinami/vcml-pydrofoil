from _pydrofoilcapi_cffi import ffi
import _pydrofoil

import sys

sys.modules['__main__'] = type(sys)('__main__')

all_cpu_handles = []

# This file is compiled INTO libpydrofoilcapi_cffi.so. Editing it here has no
# effect until that .so is rebuilt from this gluecode.py. Isolated hart copies
# under /tmp/isolated_libs are overwritten from the rebuilt .so at launch.

# Every hart dlmopen's its own copy of this module, so nothing declared here is
# shared between harts. That is why LR/SC reservations and the read-modify-write
# live in the VP instead, see atomic_mem() in sysc_vp/src/memory_callbacks.cpp.

_ABI_GPR = (
    'zero', 'ra', 'sp', 'gp', 'tp', 't0', 't1', 't2',
    's0', 's1', 'a0', 'a1', 'a2', 'a3', 'a4', 'a5',
    'a6', 'a7', 's2', 's3', 's4', 's5', 's6', 's7',
    's8', 's9', 's10', 's11', 't3', 't4', 't5', 't6',
)


def _u32(x):
    return int(x) & 0xffffffff


def _gpr_names(n):
    yield 'x%d' % n
    yield _ABI_GPR[n]


def _read_gpr(cpu, n):
    if n == 0:
        return 0
    last = None
    for name in _gpr_names(n):
        try:
            return _u32(cpu.cpu.read_register(name))
        except Exception as e:
            last = e
    raise last


def _write_gpr(cpu, n, value):
    if n == 0:
        return
    value = _u32(value)
    last = None
    for name in _gpr_names(n):
        try:
            cpu.cpu.write_register(name, value)
            return
        except Exception as e:
            last = e
    raise last


def _fetch_insn(cpu, pc_val):
    for base, size, memory in cpu.dma_regions:
        if base <= pc_val and (pc_val + 4) <= (base + size):
            ptr = ffi.cast('uint32_t*', memory + (pc_val - base))
            return int(ptr[0]) & 0xffffffff
    if getattr(cpu, 'pyread', None) is None:
        return None
    return _u32(cpu.pyread(pc_val, 4))


def _emulate_atomic(cpu, pc_val, insn):
    """Run one A-extension insn in the VP instead of in the Sail model.

    Only the register file is reachable from here, so this side decodes rd/rs1/
    rs2 and the VP decides what the instruction means.
    """
    if (insn & 0x7f) != 0x2f:
        return False
    if cpu.atomic_cb is None:
        return False

    rd = (insn >> 7) & 0x1f
    rs1 = (insn >> 15) & 0x1f
    rs2 = (insn >> 20) & 0x1f
    
    print(f"[I] atomic insn {insn:08x} at pc {pc_val:08x}, rd={rd}, rs1={rs1}, rs2={rs2}")

    result = cpu.atomic_result
    if cpu.atomic_cb(cpu._handle, insn, _read_gpr(cpu, rs1), _read_gpr(cpu, rs2), result,
                     cpu.atomic_payload) != 0:
        return False

    if not cpu.atomic_logged:
        cpu.atomic_logged = True
        print('[I] atomics are handled by the VP (rebuild the .so if this line never appears)')

    _write_gpr(cpu, rd, result[0])
    cpu.cpu.write_register('pc', pc_val + 4)
    cpu.steps += 1
    return True


class C:
    def __init__(self, rv64, n=None):
        self.rv64 = rv64
        self.arg = n
        self.callbacks = None
        self.dma_regions = []  # list of (base_address, size, memory_buffer)
        self.breakpoints = []  # list of breakpoints
        self.verbosity = True
        self.pyread = None
        self.pywrite = None
        self.atomic_cb = None
        self.atomic_payload = ffi.NULL
        self.atomic_result = ffi.new('uint64_t[1]')
        self.atomic_logged = False
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
            
        
        def dma_lookup(addr, ptr_type):
            for base, size, memory in self.dma_regions:
                if base <= addr < base + size:
                    offset = addr - base
                    return ffi.cast(ptr_type, memory + offset)
            return None

        def pyread(addr, width):
            addr = int(addr)
            
            # --- DEBUG CHECK: Abfangen von Lesezugriffen auf Adresse 0 ---
            if 0 <= addr <= 1000:
                pc_val = int(self.cpu.read_register('pc'))
                hart_id = int(self.cpu.read_register('mhartid'))
                mtvec = int(self.cpu.read_register('mtvec'))
                mepc = int(self.cpu.read_register('mepc'))
                mcause = int(self.cpu.read_register('mcause'))
                print(f"[FATAL] Hart {hart_id} versucht von Adresse 0x0 zu lesen! Aktueller PC: {hex(pc_val)}")
                print(f"        mtvec  (Trap Vector) : {hex(mtvec)}")
                print(f"        mepc   (Vorheriger PC) : {hex(mepc)}")
                print(f"        mcause (Warum?)      : {hex(mcause)}")
            # -------------------------------------------------------------

            ptr_type, bitv_size = resolve_width(width)

            # Check DMA regions first
            ptr = dma_lookup(addr, ptr_type)
            if ptr is not None:
                return _pydrofoil.bitvector(bitv_size, ptr[0])

            # Fall back to callback
            res = self.read(self._handle, addr, width, ffi.cast(ptr_type, self.mem), payload)
            assert res == 0
            return _pydrofoil.bitvector(bitv_size, self.mem[0])
        
        def pywrite(addr, width, value):
            addr = int(addr)
            value = int(value)
            ptr_type, bitv_size = resolve_width(width)

            # --- DEBUG CHECK: Abfangen von illegalen Schreibzugriffen ---
            if 0 <= addr <= 1000:
                pc_val = int(self.cpu.read_register('pc'))
                hart_id = int(self.cpu.read_register('mhartid'))
                mtvec = int(self.cpu.read_register('mtvec'))
                mepc = int(self.cpu.read_register('mepc'))
                mcause = int(self.cpu.read_register('mcause'))
                
                print(f"[FATAL WRITE] Hart {hart_id} versucht Wert {hex(value)} auf Adresse {hex(addr)} zu schreiben!")
                print(f"              Aktueller PC: {hex(pc_val)}")
                print(f"              mtvec: {hex(mtvec)}, mepc: {hex(mepc)}, mcause: {hex(mcause)}")
            # -------------------------------------------------------------

            # Check DMA regions first
            ptr = dma_lookup(addr, ptr_type)
            if ptr is not None: # How useful can it be if we're not using ptr afterwards?
                ptr[0] = value
                return

            # Fall back to callback
            res = self.write(self._handle, addr, width, value, payload)
            if res != 0:
                print("\n" + "="*60)
                try:
                    hart = int(self.cpu.read_register('mhartid'))
                    pc = int(self.cpu.read_register('pc'))
                    sp = int(self.cpu.read_register('x2'))  # <--- sp ist Register x2!
                    
                    print(f"[FATAL CRASH] Hart {hart} Memory Access Failed!")
                    print(f"Ziel-Adresse : {hex(addr)}")
                    print(f"Schreib-Wert : {hex(value)}")
                    print(f"Aktueller PC : {hex(pc)}")
                    print(f"Stack (x2)   : {hex(sp)}")
                    
                    mcause = int(self.cpu.read_register('mcause'))
                    mepc = int(self.cpu.read_register('mepc'))
                    mtvec = int(self.cpu.read_register('mtvec'))
                    mip = int(self.cpu.read_register('mip'))
                    mie = int(self.cpu.read_register('mie'))
                    print(f"mcause       : {hex(mcause)}")
                    print(f"mepc         : {hex(mepc)}")
                    print(f"mtvec        : {hex(mtvec)}")
                    print(f"mip (Pending): {hex(mip)}")
                    print(f"mie (Enable) : {hex(mie)}")
                except Exception as e:
                    print(f"[DEBUG] Konnte nicht alle Register lesen. Grund: {e}")
                print("="*60 + "\n")
                
                assert res == 0 # Crash auslösen


            assert res == 0

        self.pyread = pyread
        self.pywrite = pywrite
        self.callbacks = _pydrofoil.Callbacks(mem_read_intercept=pyread, mem_write_intercept=pywrite)

    def set_verbosity(self, verbosity):
        self.verbosity = verbosity
        self.cpu.set_verbosity(verbosity)

    def step(self):
        self.steps += 1
        self.cpu.step()
        # print(f"[DEBUG Python] self.cpu type: {type(self.cpu)}")

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
        self.set_verbosity(self.verbosity)
        self.cpu._set_htif_tohost(0x900F0000)
     
    def set_hartid(self, hartid):
        try:
            self.cpu.write_register("mhartid", hartid)
            # print(f"[Python] Successfully set mhartid to {hartid}")
        except Exception as e:
            print(f"[Python Error] Failed to set mhartid: {e}")
        # print(f"hardid in python: {self.cpu.read_register('mhartid')}")

@ffi.def_extern()
def pydrofoil_allocate_cpu(spec, fn):
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
def pydrofoil_cpu_set_atomic_callback(i, atomic_cb, payload):
    # Must be called after the ram callbacks, those reset the cpu object.
    print("[I] Setting atomic callback in Python glue code")
    cpu = ffi.from_handle(i)
    cpu.atomic_cb = atomic_cb
    cpu.atomic_payload = payload
    return 0

@ffi.def_extern()
def pydrofoil_cpu_simulate(i, steps):
    cpu = ffi.from_handle(i)
    cpu.steps = 0

    for _ in range(steps):
        # Zephyr PC auslesen und in Int konvertieren
        pc_val = int(cpu.cpu.read_register('pc'))

        # 1. Breakpoint-Check
        if cpu.breakpoints and pc_val in cpu.breakpoints:
            return cpu.steps 

        # 2. Fetch the insn once: WFI fast-forward and A-extension intercept
        insn = _fetch_insn(cpu, pc_val)
        if insn is not None:
            if insn == 0x10500073:  # RISC-V 'wfi' Opcode
                mip = int(cpu.cpu.read_register('mip'))
                mie = int(cpu.cpu.read_register('mie'))
                
                if (mip & mie) == 0:
                    # Keine Interrupts -> Core schläft -> Simulation Fast-Forward!
                    return steps
                else:
                    # Interrupt steht an -> WFI wird übersprungen (als NOP behandelt)
                    cpu.cpu.write_register('pc', pc_val + 4)
                    cpu.steps += 1
                    continue  # Gehe zum nächsten Befehl, Sail sieht das WFI nie!

            if _emulate_atomic(cpu, pc_val, insn):
                continue

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

@ffi.def_extern()
def pydrofoil_set_interrupt_pending(i, value):
    cpu = ffi.from_handle(i)
    bit_size = 64 if cpu.rv64 else 32

    # value encodes the mip bit index in bits 0..7 and the assert/deassert
    # flag in bit 8, so clearing MTIP/MEIP no longer wipes MSIP.
    value = int(value)
    bit = value & 0xff
    set_bit = (value & 0x100) != 0

    # Read the current mip so we don't destroy other pending interrupts
    current_mip = int(cpu.cpu.read_register('mip'))

    if set_bit:
        new_mip = current_mip | (1 << bit)
    else:
        new_mip = current_mip & ~(1 << bit)

    cpu.cpu.write_register('mip', _pydrofoil.bitvector(bit_size, new_mip))

    # Read CSRs for debug
    mstatus = cpu.cpu.lowlevel.read_CSR(0x300)
    mie = cpu.cpu.lowlevel.read_CSR(0x304)
    mip = cpu.cpu.lowlevel.read_CSR(0x344)
    
    # Use an f-string for atomic printing to prevent thread interleaving
    print(f"mip bit {bit} {'set' if set_bit else 'clear'}, mstatus, mie, mip: "
          f"{hex(mstatus)} {hex(mie)} {hex(mip)}")
    
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
    cpu.dma_regions.append((base_address, size, memory))
    return 0

@ffi.def_extern()
def pydrofoil_set_hartid(handle, hartid):
    cpu = ffi.from_handle(handle)
    cpu.set_hartid(hartid)
    return 0


sys.modules['__main__'].__dict__.update(globals())
sys.argv = ['embedded-pypy']