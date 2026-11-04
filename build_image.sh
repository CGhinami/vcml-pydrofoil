#!/usr/bin/env bash
set -e

# Absolute path of the directory containing the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

# Every pydrofoil/<name>/ (unpacked plugin tarball) ends up in the image
if ! compgen -G "$SCRIPT_DIR/pydrofoil/*/bin/pypy3" > /dev/null; then
	echo "No Pydrofoil plugin in $SCRIPT_DIR/pydrofoil/<name>/bin. Exiting..."
	exit 1
fi
echo "Pydrofoil plugins: $(cd "$SCRIPT_DIR/pydrofoil" && ls -d */ | tr -d / | tr '\n' ' ')"

IMAGE_NAME="vcml-pydrofoil:latest"

#Build container image, docker now processes the Dockerfile
echo "Building image..."
$CONTAINER_PROGRAM build -t "$IMAGE_NAME" "$SCRIPT_DIR"

echo
echo "Image successfully built:"
echo "  $IMAGE_NAME"