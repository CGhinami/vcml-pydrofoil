.PHONY: all clean

all: sysc_vp

# Makefile has only 1 way to expand variables or expressions, and this is with $(), eg, $(func arg1, arg2):
# $(...) -> expand this, if -> call the builtin function if
# 1st arg: $(PROFILING)
# 2nd arg (the "then"): -DCMAKE_CXX_FLAGS=-DPROFILING=$(PROFILING) which defines the macro PROFILING for the preprocessor
# 3rd arg (the "else"): empty
sysc_vp:
	@echo "Building the SystemC VP"
	@mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug $(if $(PROFILING),-DCMAKE_CXX_FLAGS=-DPROFILING=$(PROFILING)) .. && make -j24

clean:
	@echo "Cleaning the build directory"
	@if [ -d "build" ]; then rm -rf build; fi
