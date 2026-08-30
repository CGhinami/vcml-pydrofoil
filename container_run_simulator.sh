#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default container program
CONTAINER_PROGRAM="docker"
CFG_FILE=""
DEBUG_CMD=""
PTRACE_FLAG=""

# 1. Check for the debug flag FIRST
if [[ "$1" == "-d" || "$1" == "--debug" ]]; then
    DEBUG_CMD="-d"
    # GDB inside a container requires SYS_PTRACE capabilities
    PTRACE_FLAG="--cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
    shift
fi

# 2. Check engine and config
if [[ "$1" == "docker" || "$1" == "podman" ]]; then
    CONTAINER_PROGRAM="$1"
    CFG_FILE="$2"
else
    # If it's not a container program, treat the first argument as the config file
    CFG_FILE="$1"
fi

if [[ "$CONTAINER_PROGRAM" == "docker" ]]; then
    if command -v docker &> /dev/null; then
        CONTAINER_PROGRAM_FLAGS="--user $(id -u):$(id -g) $PTRACE_FLAG"
        echo "Using docker"
    else
        echo "Docker was selected but it is not installed. Exiting..."
        exit 1
    fi
elif [[ "$CONTAINER_PROGRAM" == "podman" ]]; then
    if command -v podman &> /dev/null; then
        CONTAINER_PROGRAM_FLAGS="--userns keep-id $PTRACE_FLAG"
        echo "Using podman"
    else
        echo "Podman was selected but it is not installed. Exiting..."
        exit 1
    fi
fi

# Alma needs :z to bind the mount, otherwise this results in "permission denied"
MOUNT_RO_OPTS="ro"
if command -v getenforce &> /dev/null && [[ "$(getenforce)" != "Disabled" ]]; then
    MOUNT_RO_OPTS="ro,z"
fi

IMAGE_NAME="vcml-pydrofoil:latest"

# Detects if the script is running in an interactive terminal (TTY)
INTERACTIVE_FLAGS="-i"
if [[ -t 0 ]]; then
    INTERACTIVE_FLAGS="-it"
fi

# Run the container
if [[ -n "$CFG_FILE" ]]; then
    CFG_REL_PATH="$(realpath --relative-to="$SCRIPT_DIR" "$(realpath "$CFG_FILE")")"

    $CONTAINER_PROGRAM run \
        $CONTAINER_PROGRAM_FLAGS \
        --rm \
        $INTERACTIVE_FLAGS \
        -v "$SCRIPT_DIR:/configs:$MOUNT_RO_OPTS" \
        "$IMAGE_NAME" \
        $DEBUG_CMD "$CFG_REL_PATH"
else
    $CONTAINER_PROGRAM run \
        $CONTAINER_PROGRAM_FLAGS \
        --rm \
        $INTERACTIVE_FLAGS \
        "$IMAGE_NAME" $DEBUG_CMD
fi