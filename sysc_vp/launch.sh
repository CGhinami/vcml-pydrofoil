#!/bin/bash

# without gdb: ./launch.sh <path to myconfig.cfg>
# with gdb: ./launch.sh -d <path to myconfig.cfg>

PYDROFOIL_BIN_DIR="../pypy-pydrofoil-scripting-experimental/bin"
VP_BINARY="./build/sysc_vp"
VP_BENCHMARK="./benchmark"
DEBUG=false

# shift removes the debug flag from the args --> $1 now should be the cfg file
if [[ "$1" == "-d" || "$1" == "--debug" ]]; then
    DEBUG=true
    shift
fi

# check if the $1 string is empty
# $0 is the script name.
if [[ -z "$1" ]]; then
    echo "Usage: $0 [-d|--debug] <config-file>"
    exit 1
fi

VP_CFG=$1

# Verify that the file exists
if [[ ! -f "$VP_CFG" ]]; then
    echo "Config file not found: $VP_CFG"
    exit 1
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"$PYDROFOIL_BIN_DIR"

if $DEBUG; then
    echo "Running in debug mode (gdb)…"
    if [[ ! -f "$VP_BENCHMARK"/gdb_vp_cmd.gdb ]]; then 
        gdb --args "$VP_BINARY" -f "$VP_CFG"
    else
        gdb -x "$VP_BENCHMARK"/gdb_vp_cmd.gdb --args "$VP_BINARY" -f "$VP_CFG"
    fi
else
    echo "Running normally…"
    "$VP_BINARY" -f "$VP_CFG"
fi