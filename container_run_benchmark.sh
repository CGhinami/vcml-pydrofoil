#!/usr/bin/env bash
# Runs benchmark/benchmark.py inside the vcml-pydrofoil image (build_image.sh).
#
#   ./container_run_benchmark.sh [docker|podman] <sweep.json> [benchmark.py options]
#   ./container_run_benchmark.sh benchmark/sweeps/dhrystone.json \
#       --dhrystone-elfs ../multicore_test/dhrystone/benchmarks
#   ./container_run_benchmark.sh benchmark/sweeps/linux_npb.json \
#       --images ../Pydrofoil-RISCV-benchmarks/sw/images/buildroot
#
# The VP (build/sysc_vp, gluecode, Pydrofoil plugin) comes from the image. The
# host's benchmark/ (sweeps, configs) is mounted over the image's, so they need
# no image rebuild; results/ is mounted writable, so the CSV and the logs end up
# on the host. The input folders (--images, --zephyr-elfs, --dhrystone-elfs)
# are mounted read-only, they and --out are rewritten to container paths.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="vcml-pydrofoil:latest"
CONTAINER_DIR="/vcml-pydrofoil"

CONTAINER_PROGRAM="docker"
if [[ "$1" == "docker" || "$1" == "podman" ]]; then
    CONTAINER_PROGRAM="$1"
    shift
fi
if [[ -z "$1" ]]; then
    echo "Usage: $0 [docker|podman] <sweep.json> [benchmark.py options]"
    exit 1
fi

if [[ "$CONTAINER_PROGRAM" == "docker" ]]; then
    command -v docker &> /dev/null || { echo "docker is not installed. Exiting..."; exit 1; }
    CONTAINER_PROGRAM_FLAGS="--user $(id -u):$(id -g)"
else
    command -v podman &> /dev/null || { echo "podman is not installed. Exiting..."; exit 1; }
    CONTAINER_PROGRAM_FLAGS="--userns keep-id"
fi

mkdir -p "$SCRIPT_DIR/results"
MOUNTS=(-v "$SCRIPT_DIR/benchmark:$CONTAINER_DIR/benchmark:ro,Z"
        -v "$SCRIPT_DIR/results:$CONTAINER_DIR/results:Z")

# A host path inside the repository as seen in the container, empty otherwise
in_repo() {
    local abs
    abs="$(realpath -m "$1")"
    case "$abs" in
        "$SCRIPT_DIR"/*) echo "$CONTAINER_DIR/${abs#"$SCRIPT_DIR"/}" ;;
        *) echo "" ;;
    esac
}

# The sweep file: from the mounted benchmark/ if it lies in the repository,
# otherwise its directory is mounted on its own
SWEEP="$1"
shift
[[ -f "$SWEEP" ]] || { echo "Sweep file not found: $SWEEP"; exit 1; }
SWEEP_IN=$(in_repo "$SWEEP")
case "$SWEEP_IN" in
    "$CONTAINER_DIR"/benchmark/*) ;;
    *)
        MOUNTS+=(-v "$(dirname "$(realpath "$SWEEP")"):/sweeps:ro,Z")
        SWEEP_IN="/sweeps/$(basename "$SWEEP")"
        ;;
esac

ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --images|--zephyr-elfs|--dhrystone-elfs)
            # input folders of the benchmarks: mounted read-only under /inputs
            [[ -d "$2" ]] || { echo "$1: directory not found: $2"; exit 1; }
            MOUNTS+=(-v "$(realpath "$2"):/inputs/${1#--}:ro,Z")
            ARGS+=("$1" "/inputs/${1#--}")
            shift 2
            ;;
        --out)
            OUT_IN=$(in_repo "$2")
            case "$OUT_IN" in
                "$CONTAINER_DIR"/results/*) ARGS+=(--out "$OUT_IN") ;;
                *) echo "--out has to be inside $SCRIPT_DIR/results (the mounted directory)"; exit 1 ;;
            esac
            shift 2
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

INTERACTIVE_FLAGS="-i"
[[ -t 0 ]] && INTERACTIVE_FLAGS="-it"

exec $CONTAINER_PROGRAM run \
    $CONTAINER_PROGRAM_FLAGS \
    --rm \
    $INTERACTIVE_FLAGS \
    "${MOUNTS[@]}" \
    -w "$CONTAINER_DIR" \
    --entrypoint python3 \
    "$IMAGE_NAME" \
    benchmark/benchmark.py "$SWEEP_IN" "${ARGS[@]}"
