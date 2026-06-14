#include "system.h"
#include "pydrofoilcapi.h"
#include "core.h" 

extern "C" int sc_main(int argc, char **argv) {
    int exit_code = 0;

    // Wir öffnen einen künstlichen Scope (Sichtbarkeitsbereich)
    {
        class system system("system");

        // Simulation läuft...
        exit_code = system.run();
        
    } // <--- WICHTIG: Hier geht "system" out of scope! 
      // C++ ruft JETZT alle Destruktoren (~PydrofoilCore) auf.
      // Der Python-Worker lebt noch und kann "FreeCpu" sauber abarbeiten!

    // Jetzt, wo alle Kerne zerstört und aus Python abgemeldet sind,
    // können wir den globalen Thread sicher beenden.
    PydrofoilCore::shutdown_worker();

    return exit_code;
}