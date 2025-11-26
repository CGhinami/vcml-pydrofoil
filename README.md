# Pydrofoil integration in a SystemC-TLM2.0 environment

This projects is an integration of the Pydrofoil ISS into the SystemC-TLM2.0 environment.<br/>
The two parts, Pydrofoil and SystemC-TLM, communicate through a C-based API.

## 1) Build the project

1) Download the Pydrofoil-PyPy plugin from [this link](https://github.com/pydrofoil/pydrofoil/actions/workflows/plugin.yml)

2) Execute the build.sh script:<br/>
    **The script expects the downloaded plugin to be in the same directory of the build.sh**.
    ```
    cd ~/vcml-pydrofoil
    ./build.sh
    ```
    This will:<br/>
    - Generate C-based glue code that exposes python functions (`gluecode.py` code ends up in `_pydrofoilcapi_cffi.c`)<br/>
    - Compile the generated C glue code to an object file (`_pydrofoilcapi_cffi.o`)<br/>
    - Compile a test C file (`testmain.c`) that uses the API functions<br/>
    - Link both C files with the PyPy runtime library (`libpypy3.11-c.so`)<br/>
    - Execute the test file (test main() -> API function -> PyPy interpreter runs python ISS)<br/>

3) Build the SystemC environment:<br/>
    ```
    cd ~/vcml-pydrofoil/sysc_vp
    make # Use make PROFILING=1 if interested in profiling information, see below
    ```

## 2) Run the simulator
To run the simulator use:
```
cd ~/vcml-pydrofoil/sysc_vp
./launch.sh <path to the simulator .cfg>

# Only for debugging:
./launch.sh -d <path to the simulator .cfg>
```

## 3) Profile the SystemC-TLM2.0 
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