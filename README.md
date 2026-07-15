# Pydrofoil integration in a SystemC-TLM2.0 environment

This projects is an integration of the Pydrofoil ISS into the SystemC-TLM2.0 environment.<br/>
The two parts, Pydrofoil and SystemC-TLM, communicate through a C API.

## Quick Start

1) Get the pre-built VP image from [here](https://github.com/CGhinami/vcml-pydrofoil/pkgs/container/vcml-pydrofoil)

2) Run the image. Both docker and podman options are supported. The docker option is the default one.
   ```bash
   cd ~/vcml-pydrofoil
   # To run it with podman
   ./container_run_simulation.sh podman
   # To run it with docker
   ./container_run_simulation.sh # or
   ./container_run_simulation.sh docker
   ```
    The run will by default run the [64-bit addi riscv](benchmark/rv64_addi.elf) example. If you want to run a different application, pass the configuration file as an input to the run script:
    ```bash
   ./container_run_simulation.sh podman <path to the configuration file>
   ```
    See below more details on how to write your own configuration file and how to run your custom application.

## Repo Organization

| Directory | Description |
| --------- | ----------- |
| [benchmark](benchmark) | SW samples to test vcml-pydrofoil on. |
| [sysc_vp](sysc_vp) | The SystemC wrapper that uses the Pydrofoil emulator. |


## Basics on how to configure the VP

The VP's configuration file allows you to modify some properties about the VP without the need to recompile the project. [This](benchmark/riscv64_ex.cfg) is an example of a configuration file. In the table below, we report some useful properties that can be set in the file:

| Property | Options | Description |
| --------- | --------- | ----------- |
| `system.cpu.gdb_wait` | `true`, `false` | When set to true, we VP waits for a gdb connection on the specified port. |
| `system.core.gdb_port` | 1–65535 | The gdb port number. |
| `system.core.arch_name` | `rv64`, `rv32` | The architecture type. |
| `system.core.pc` | Any value within the RAM | The program counter value where to start the execution from. |
| `system.core.symbols` | - | Path to the SW sample .elf file. |
| `system.ram.images` | - | Path to the SW sample .bin file. |
| `system.ram.size` | 64 KiB - 16 GiB | Size of the RAM. |
| `system.addr_ram` | - | RAM Physical address. |
| `system.addr_uart0` | - | UART Physical address. |
| `system.term.backends` | `tui`, `file`, `tcp:<tcp port>`, `null` | The terminal backend. When `file` is selected, the data to be sent must be available in a file named *system.term.rx*. The UART sends the data to the *system.term.tx* file. |
| `system.duration` |  Integer values with suffixes s, ms, us or ns | simulation duration. Simulation will stop automatically once this time-stamp is reached. If you want to simulate infinitely, do not use this in the file. |


## Build the project from source

1) Clone this repository with the `--recursive` flag to initialize the submodules.

2) Download the Pydrofoil-PyPy plugin from [this link](https://github.com/pydrofoil/pydrofoil/actions/workflows/plugin.yml) and untar it. The framework expects this to be inside the `vcml-pydrofoil` project directory.


3) Build the project

    -**With** container support:

    ```bash
    cd ~/vcml-pydrofoil
    # This will use docker by default, specify podman to use it
    ./build_image.sh
    ```

    This will:<br/>
    - Build the image <br/>
    - Generate C-based glue code that exposes python functions ([gluecode.py](gluecode.py) is emitted in the *_pydrofoilcapi_cffi.c* generated file)<br/>
    - Compile the generated C glue code to an object file (*_pydrofoilcapi_cffi.o*)<br/>
    - Compile a test C file ([testmain.c](testmain.c)) that uses the API functions<br/>
    - Link both C files with the PyPy runtime library (*libpypy3.11-c.so*)<br/>
    - Build the SystemC environment <br/>

    -**Without** container support:
    ```bash
    cd ~/vcml-pydrofoil
    ./build_sim.sh
    ```

## Run the simulator

Run [container_run_simulator.sh](container_run_simulator.sh) to run **with** container support. This will use docker by default, specify podman to use it.

Run [launch.sh](launch.sh) to run **without** container support.

Specify the configuration file if you do not want to run the default [riscv64 example](benchmark/riscv64_ex.cfg):
```bash
cd ~/vcml-pydrofoil
# To run it with podman
./container_run_simulation.sh podman <path to custom .cfg file>
# To run it with docker
./container_run_simulation.sh <path to custom .cfg file>
```

*Note:*
The simulator supports RTOS and bare-metal applications. Linux applications are still under test. The current simulator has only the UART peripheral connected, but the RISCV-compatible peripherals from [VCML](https://github.com/machineware-gmbh/vcml) are supported. To add them, modify the files where the peripherals are instantiated and connected ([system.cpp](sysc_vp/src/system.cpp) and [system.h](sysc_vp/include/system.h)).


## Profile the SystemC-TLM2.0
Requirements: *perf* and *hotspot*

If interested in the performance of the SystemC-TLM side, it is possible to use perf.<br/>
Perf is part of the Linux kernel performance tools, while hotspot is a graphical performance analysis built on top of Linux perf.<br/>
On Debian/Ubuntu:<br/>
```
sudo apt update
sudo apt install linux-tools-common linux-tools-generic hotspot
```
Then run with:<br/>
```
# This creates the perf.data file
sudo LD_LIBRARY_PATH=<path to pypy-pydrofoil-scripting-experimental/bin/> \
    perf record --call-graph dwarf <path to sysc_vp executable> -f "<path to simulator .cfg>"

# In my machine, it looks like:
# sudo LD_LIBRARY_PATH=~/vcml-pydrofoil/pypy-pydrofoil-scripting-experimental/bin/ \
#      perf record --call-graph dwarf ./build/sysc_vp -f "benchmark/riscv64_ex.cfg"

# Allow hotspot to read the file
sudo chmod 444 perf.data

# Display the data in a call graph
sudo hotspot
```
When running on a remote Linux machine with SSH we need to enable X11 forwarding.<br/>
X11 forwarding is an SSH feature that allows graphical applications running on the machine to display their windows on the local computer.<br/>
If this is your case, SSH into the remote machine with forwarding enabled:
```
ssh -X user@remote_host
```
In my machine, this looks like: `ssh -X ghinami@serious`

Call perf normally:
```
sudo LD_LIBRARY_PATH=<path to pypy-pydrofoil-scripting-experimental/bin/> \
    perf record --call-graph dwarf <path to sysc_vp executable> -f "<path to simulator .cfg>"
```
and call hotspot with the -E flag to preserve the environmental variables, normally stripped by sudo:
```
sudo chmod 444 perf.data
sudo -E hotspot
```
*Note* although perf has been recently enhanced to support python profiling, our python interpreter does not support it. To know how much each python function is taking, build the project with `make PROFILING=1` and launch it saving the output in a file, eg, `./launch.sh benchmark/riscv64_ex.cfg > prof_out.txt`