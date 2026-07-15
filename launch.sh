#!/bin/bash

# without gdb: ./launch.sh <path to myconfig.cfg>
# with gdb: ./launch.sh -d <path to myconfig.cfg>

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDROFOIL_BIN_DIR="$SCRIPT_DIR/pypy-pydrofoil-scripting-experimental/bin"
VP_BINARY="$SCRIPT_DIR/build/sysc_vp"
VP_BENCHMARK="$SCRIPT_DIR/benchmark"

# Set the DEBUG var if it's not already set
DEBUG=${DEBUG:-0}

# shift removes the debug flag from the args --> $1 now should be the cfg file
if [[ "$1" == "-d" || "$1" == "--debug" ]]; then
    DEBUG=1
    shift
fi

# check if the $1 string is empty
# $0 is the script name.
if [[ -z "$1" ]]; then
    echo "Usage: $0 [-d|--debug] <config-file>"
    exit 1
fi

VP_CFG=$1

if [[ -f "/configs/$VP_CFG" ]]; then
    VP_CFG="/configs/$VP_CFG"
elif [[ ! -f "$VP_CFG" ]]; then
    echo "Config not found: $VP_CFG"
    exit 1
fi

if [[ ! -f "$VP_BINARY" ]]; then
    echo "Binary not found: $VP_BINARY"
    exit 1
fi


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"$PYDROFOIL_BIN_DIR"

if [[ "$DEBUG" == "1" ]]; then
    echo "Running in debug mode (gdb)…"
    if [[ ! -f "$VP_BENCHMARK"/gdb_vp_cmd.gdb ]]; then
        gdb --args "$VP_BINARY" -f "$VP_CFG"
    else
        gdb -x "$VP_BENCHMARK"/gdb_vp_cmd.gdb --args "$VP_BINARY" -f "$VP_CFG"
    fi
else
    echo "Running normally…"
    #"$VP_BINARY" --trace-stdout -f "$VP_CFG"
    "$VP_BINARY" -f "$VP_CFG"
fi