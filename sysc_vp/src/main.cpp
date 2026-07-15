/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "system.h"
#include "pydrofoilcapi.h"

extern "C" int sc_main(int argc, char** argv)
{
    class virtual_platform::system system("system");

    return system.run();
}
