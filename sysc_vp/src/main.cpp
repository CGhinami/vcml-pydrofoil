#include "system.h"
#include "pydrofoilcapi.h"
#include "core.h" // <-- WICHTIG: Damit sc_main PydrofoilCore kennt

extern "C" int sc_main(int argc, char **argv) {

    class system system("system");

    // 1. Starte die Simulation. Das Programm bleibt hier stehen, bis die Simulation vorbei ist.
    int exit_code = system.run();

    // 2. Die Simulation ist beendet! 
    // Jetzt sagen wir dem globalen Python-Worker-Thread, dass er sich beenden soll.
    PydrofoilCore::shutdown_worker();

    // 3. SystemC sauber beenden
    return exit_code;
}