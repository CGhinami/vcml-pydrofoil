# Compile it with:
# make
# Run it with: 
# cd ~/vcml-pydrofoil/sysc_vp
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../pypy-pydrofoil-scripting-experimental/bin
# gdb --args ./build/sysc_vp -f benchmark/riscv64_ex.cfg OR
# ./build/sysc_vp -f "benchmark/riscv64_ex.cfg"
.PHONY: all clean

all: sysc_vp

sysc_vp:
	@echo "Building the SystemC VP"
	@mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j24

clean:
	@echo "Cleaning the build directory"
	@if [ -d "build" ]; then rm -rf build; fi
