# LD_LIBRARY_PATH=/home/seibt/thesis/vcml-pydrofoil/pypy-pydrofoil-scripting-experimental/bin gdb -x debug.gdb --args ./build/sysc_vp -f benchmark/riscv64_ex.cfg
break main
b processor::processor_thread_async
# break system::system
# break PydrofoilCore::PydrofoilCore
# break PydrofoilCore::simulate
run