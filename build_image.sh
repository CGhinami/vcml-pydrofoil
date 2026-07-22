#!/usr/bin/env bash
set -e

# Absolute path of the directory containing the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDROFOIL_BIN_DIR="$SCRIPT_DIR/pypy-pydrofoil-scripting-experimental/bin"

# If user specified a preference, try it. Use docker otherwise
CONTAINER_PROGRAM=${1:-docker}

if [[ "$CONTAINER_PROGRAM" == "docker" ]]; then

	if command -v docker &> /dev/null; then
		echo "Using docker"
	else
		echo "Docker was selected but it is not installed. Exiting..."
		exit 1
	fi

elif [[ "$CONTAINER_PROGRAM" == "podman" ]]; then

	if command -v podman &> /dev/null; then
		echo "Using podman"
	else
		echo "Podman was selected but it is not installed. Exiting..."
		exit 1
	fi
else
	echo "Invalid containerization option selected. Exiting..."
	echo "Usage: $0 [docker|podman]"
	exit 1
fi

if [[ ! -d "$PYDROFOIL_BIN_DIR" ]]; then
	echo "Pydrofoil bin not found in: "$PYDROFOIL_BIN_DIR". Exiting..."
	exit 1
fi

IMAGE_NAME="vcml-pydrofoil:latest"

#Build container image, docker now processes the Dockerfile
echo "Building image..."
$CONTAINER_PROGRAM build -t "$IMAGE_NAME" "$SCRIPT_DIR"

echo
echo "Image successfully built:"
echo "  $IMAGE_NAME"