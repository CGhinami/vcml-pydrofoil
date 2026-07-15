#!/usr/bin/env bash
set -e

# Default container program
CONTAINER_PROGRAM="docker"
CFG_FILE=""

# Check if the first argument is explicitly 'docker' or 'podman'
if [[ "$1" == "docker" || "$1" == "podman" ]]; then
    CONTAINER_PROGRAM="$1"
    CFG_FILE="$2"
else
    # If it's not a container program, treat the first argument as the config file
    CFG_FILE="$1"
fi

if [[ "$CONTAINER_PROGRAM" == "docker" ]]; then

	if command -v docker &> /dev/null; then
		CONTAINER_PROGRAM_FLAGS="--user $(id -u):$(id -g)"
		echo "Using docker"
	else
		echo "Docker was selected but it is not installed. Exiting..."
		exit 1
	fi

elif [[ "$CONTAINER_PROGRAM" == "podman" ]]; then

	if command -v podman &> /dev/null; then
		CONTAINER_PROGRAM_FLAGS="--userns keep-id"
		echo "Using podman"
	else
		echo "Podman was selected but it is not installed. Exiting..."
		exit 1
	fi
fi

IMAGE_NAME="vcml-pydrofoil:latest"

# Detects if the script is running in an interactive terminal (TTY)
# If a TTY exists, use '-it'. If not (like in CI), only use '-i'
INTERACTIVE_FLAGS="-i"
if [[ -t 0 ]]; then
    INTERACTIVE_FLAGS="-it"
fi

# If the cfg file was provided, this overwrites the one specified in the Dockerfile
if [[ -n "$CFG_FILE" ]]; then
    CFG_BASENAME=$(basename "$CFG_FILE")

    $CONTAINER_PROGRAM run \
        $CONTAINER_PROGRAM_FLAGS \
        --rm \
        $INTERACTIVE_FLAGS \
        -v "$(dirname "$(realpath "$CFG_FILE")"):/configs:ro" \
        "$IMAGE_NAME" \
        "$CFG_BASENAME"
else
    $CONTAINER_PROGRAM run \
        $CONTAINER_PROGRAM_FLAGS \
        --rm \
        $INTERACTIVE_FLAGS \
        "$IMAGE_NAME"
fi