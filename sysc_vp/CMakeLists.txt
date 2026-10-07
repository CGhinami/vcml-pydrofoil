cmake_minimum_required(VERSION 3.12)
project(sysc_vp VERSION 2025.07.31 LANGUAGES C CXX)

# Look into the cmake/ subdir when searching for a custom module
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(FindGithubRepo)

get_filename_component(PYDROFOIL_LIB_DIR "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
message(STATUS "PYDROFOIL_LIB_DIR=${PYDROFOIL_LIB_DIR}")

# Normally, we would create a library for those, install it
# and import it using find_package(), here it seems that
# find_github_repo takes care of this for us, those sets
# are somehow used by find_github_repo()
set(DEPS_DIR_GENERAL ${PROJECT_SOURCE_DIR}/deps)
set(VCML_HOME ${DEPS_DIR_GENERAL}/vcml)
set(SYSTEMC_HOME ${DEPS_DIR_GENERAL}/systemc)
set(MWR_HOME ${DEPS_DIR_GENERAL}/mwr)

# Not enabling the install for now
#set(ABSL_ENABLE_INSTALL ON)
find_github_repo(mwr "machineware-gmbh/mwr")
find_github_repo(vcml "machineware-gmbh/vcml")

set(SRC ${PROJECT_SOURCE_DIR}/src)

add_executable(sysc_vp
    ${SRC}/system.cpp
    ${SRC}/main.cpp
    ${SRC}/core.cpp
    ${SRC}/python_tasks.cpp
    ${SRC}/memory_callbacks.cpp
    ${SRC}/uart_injector.cpp
    ${SRC}/multicore_simdev.cpp
    ${SRC}/pydrofoil_lib.cpp
)

# Every hart dlmopen()s its own copy of the Pydrofoil C API (see
# pydrofoil_lib.h), so it is not linked; this is the default path of the
# system.coreN.pydrofoil_lib property. Its libpypy3.11-c.so dependency is
# found through LD_LIBRARY_PATH (set by launch.sh).
target_compile_definitions(sysc_vp PRIVATE
    PYDROFOIL_CAPI_LIB="${PYDROFOIL_LIB_DIR}/libpydrofoilcapi_cffi.so"
)

# in ${PYDROFOIL_LIB_DIR} we have pydrofoilcapi.h
target_include_directories(sysc_vp PRIVATE
    ${PYDROFOIL_LIB_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${SRC}
)

# Tells the linker where to find compiled code *at compile time*
target_link_libraries(sysc_vp mwr vcml ${CMAKE_DL_LIBS})

set_target_properties(sysc_vp PROPERTIES CXX_STANDARD 17)
set_target_properties(sysc_vp PROPERTIES OUTPUT_NAME "sysc_vp")
