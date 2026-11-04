#!/usr/bin/env bash
set -e


#Every Pydrofoil plugin (unpacked pypy-pydrofoil-scripting-experimental tarball)
#lives in pydrofoil/<name>/ (old, new, ...); launch.sh picks one with PYDROFOIL=<name>.
#For each of them:
#Generate C-based glue code that exposes python functions (gluecode.py code ends up in _pydrofoilcapi_cffi.c)
#Compile the generated C glue code to an object file (_pydrofoilcapi_cffi.o)
#Link it with the plugin's PyPy runtime library (libpypy3.11-c.so) to
#pydrofoil/<name>/libpydrofoilcapi_cffi.so. The rpath (DT_RPATH, which wins over
#LD_LIBRARY_PATH) makes it load the libpypy3.11-c.so of its own plugin.
found=0
for dir in pydrofoil/*/; do
    dir=${dir%/}
    [[ -x "$dir/bin/pypy3" ]] || continue
    found=1
    echo "Building the Pydrofoil C API for $dir"
    "$dir/bin/pypy3" build_ext.py
    gcc -pthread -DNDEBUG -O2 -fPIC -I"$dir/include/pypy3.11" -c _pydrofoilcapi_cffi.c -o ./_pydrofoilcapi_cffi.o
    gcc -pthread -shared -Wl,-Bsymbolic-functions -Wl,--disable-new-dtags -Wl,-rpath,'$ORIGIN/bin' \
        ./_pydrofoilcapi_cffi.o -L"$dir/bin" -lpypy3.11-c -o "$dir/libpydrofoilcapi_cffi.so"
done
if [[ $found == 0 ]]; then
    echo "No Pydrofoil plugin in pydrofoil/<name>/bin. Exiting..."
    exit 1
fi

#Build SystemC environment
mkdir -p build
cd build

cmake ../sysc_vp \
    -DCMAKE_PREFIX_PATH="${SYSTEMC_HOME}" \
    -DCMAKE_BUILD_TYPE=Release

make -j"$(nproc)"
