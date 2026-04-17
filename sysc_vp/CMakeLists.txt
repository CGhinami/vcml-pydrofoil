cmake_minimum_required(VERSION 3.12)
project(sysc_vp VERSION 2025.07.31 LANGUAGES C CXX)

# Look into the cmake/ subdir when searching for a custom module
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(FindGithubRepo)
find_package(OpenCV REQUIRED)

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
)

# in ${PYDROFOIL_LIB_DIR} we have pydrofoilcapi.h
target_include_directories(sysc_vp PRIVATE
    ${PYDROFOIL_LIB_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${SRC}  
)

target_link_directories(sysc_vp PRIVATE
    ${PYDROFOIL_LIB_DIR}
    ${PYDROFOIL_LIB_DIR}/pypy-pydrofoil-scripting-experimental/bin/
)

# Tells the linker where to find compiled code *at compile time*
target_link_libraries(sysc_vp mwr vcml ${OpenCV_LIBS})
target_link_libraries(sysc_vp 
  pypy3.11-c
  pydrofoilcapi_cffi # The linker expects to find a shared object named libpydorofilapi_cffi.so
)

set_target_properties(sysc_vp PROPERTIES CXX_STANDARD 17)
set_target_properties(sysc_vp PROPERTIES OUTPUT_NAME "sysc_vp")
# When we build the project, the linker uses the shared library
# to check that all the symbols the sysc_vp uses are available
# in the lib. At runtime the dynamic linker loads the .so into the
# memory. Our .so lives in a custom folder, either we export LD_LIBRARY_PATH
# or we embed the .so path in the executable at build time --> not working
set_target_properties(sysc_vp PROPERTIES
  BUILD_RPATH "${PYDROFOIL_LIB_DIR}"
  INSTALL_RPATH "${PYDROFOIL_LIB_DIR}"
)
