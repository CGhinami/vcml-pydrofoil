/******************************************************************************
 *                                                                            *
 * Copyright 2026 Luis Seibt                                                  *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "pydrofoil_lib.h"
#include <dlfcn.h>

namespace backend {

std::string PydrofoilLib::load(const std::string& path)
{
    handle = dlmopen(LM_ID_NEWLM, path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if(handle == nullptr)
        return std::string("dlmopen failed: ") + dlerror();

#define PYDROFOIL_LIB_SYM(fn)                                        \
    fn = reinterpret_cast<decltype(fn)>(dlsym(handle, #fn));         \
    if(fn == nullptr)                                                \
        return std::string("missing symbol " #fn " in ") + path;
    PYDROFOIL_API(PYDROFOIL_LIB_SYM)
#undef PYDROFOIL_LIB_SYM

    return "";
}

PydrofoilLib::~PydrofoilLib()
{
    if(handle != nullptr)
        dlclose(handle);
}

} // namespace backend
