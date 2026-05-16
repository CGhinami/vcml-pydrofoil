#include "system.h"
#include "pydrofoilcapi.h"

extern "C" int sc_main(int argc, char **argv) {

  class system system("system");

  return system.run();
}