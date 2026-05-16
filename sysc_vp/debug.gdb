# LD_LIBRARY_PATH=/home/seibt/thesis/vcml-pydrofoil/pypy-pydrofoil-scripting-experimental/bin gdb -x debug.gdb --args ./build/sysc_vp -f benchmark/riscv64_ex.cfg
#break main
#b processor::processor_thread_async
# break system::system
# break PydrofoilCore::PydrofoilCore
# break PydrofoilCore::simulate
b vcml::meta::simdev::write_stop
b vcml::meta::simdev::write_exit
b vcml::meta::simdev::write_abrt
b sc_stop
b sc_simcontext::stop

run
sudo LD_LIBRARY_PATH=/home/seibt/thesis/vcml-pydrofoil/pypy-pydrofoil-scripting-experimental/bin  perf record --call-graph dwarf ./build/sysc_vp -f /home/seibt/thesis/multicore_test/bare_metal/dual_core_counter/riscv64_ex.cfg