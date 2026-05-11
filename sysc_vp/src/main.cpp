#include "system.h"
#include "pydrofoilcapi.h"
#include <cstdlib> // <-- 1. Add this for std::exit

extern "C" int sc_main(int argc, char **argv) {

    class system system("system");

    // 2. Capture the return value of the simulation
    int result = system.run(); 

    // 3. Brutally terminate the process before C++ can call ~PydrofoilCore()!
    std::exit(result); 
}