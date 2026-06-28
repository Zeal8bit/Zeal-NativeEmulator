#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
compose_file="$repo_root/docker/rpi/docker-compose.yml"

compose=${COMPOSE:-}
if [ -z "$compose" ]; then
    if command -v podman >/dev/null 2>&1; then
        compose="podman-direct"
    elif command -v podman-compose >/dev/null 2>&1; then
        compose="podman-compose"
    elif command -v podman >/dev/null 2>&1 && podman compose version >/dev/null 2>&1; then
        compose="podman compose"
    elif command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
        compose="docker compose"
    else
        echo "error: docker compose or podman compose is required" >&2
        exit 1
    fi
fi

case "$compose" in
    docker*)
        export DOCKER_BUILDKIT=${DOCKER_BUILDKIT:-1}
        export COMPOSE_DOCKER_CLI_BUILD=${COMPOSE_DOCKER_CLI_BUILD:-1}
        export COMPOSE_BAKE=${COMPOSE_BAKE:-false}
        ;;
esac

cd "$repo_root"

set_service_config() {
    case "$1" in
        build-rpi0)
            platform="linux/arm64"
            image="zeal-native-rpi0-builder"
            dockerfile="docker/rpi/Dockerfile.armhf"
            target_cflags="-mcpu=arm1176jzf-s -marm -mfpu=vfp -mfloat-abi=hard"
            build_dir="build-rpi0"
            profile="raspberrypi0-cross.build"
            setup_arg="--cross-file"
            binary="zeal-native.armv6"
            ;;
        build-rpi3)
            platform="linux/arm64"
            image="zeal-native-rpi3-builder"
            dockerfile="docker/rpi/Dockerfile.armhf"
            target_cflags="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
            build_dir="build-rpi3"
            profile="raspberrypi3-cross.build"
            setup_arg="--cross-file"
            binary="zeal-native.armv7"
            ;;
        build-rpi64)
            platform="linux/arm64"
            image="zeal-native-rpi64-builder"
            dockerfile="docker/rpi/Dockerfile.arm64"
            target_cflags=""
            build_dir="build-rpi64"
            profile="raspberrypi64-cross.build"
            setup_arg="--native-file"
            binary="zeal-native.arm64"
            ;;
        build-rpi-gpi2)
            platform="linux/arm64"
            image="zeal-native-rpi-gpi2-builder"
            dockerfile="docker/rpi/Dockerfile.arm64"
            target_cflags=""
            build_dir="build-rpi-gpi2"
            profile="raspberrypi-gpi2.build"
            setup_arg="--native-file"
            binary="zeal-native.gpi2-arm64"
            ;;
        *)
            echo "error: unknown service '$1'" >&2
            exit 1
            ;;
    esac
}

run_with_podman_direct() {
    set_service_config "$1"
    podman build \
        --platform "$platform" \
        --build-arg "TARGET_CFLAGS=$target_cflags" \
        -f "$repo_root/$dockerfile" \
        -t "$image" \
        "$repo_root" || return 1

    podman run --rm \
        --platform "$platform" \
        -v "$repo_root:/src" \
        -w /src \
        -e "MESON_ARGS=$MESON_ARGS" \
        "$image" \
        sh -lc "
            rm -rf $build_dir &&
            meson setup $build_dir $setup_arg $profile -Draylib_path=/opt/raylib-target -Dlinux_display_backend=drm \$MESON_ARGS &&
            meson compile -C $build_dir &&
            rm -rf $build_dir/lib &&
            mkdir -p $build_dir/lib &&
            cp -L /opt/raylib-target/lib/libraylib.so.550 $build_dir/lib/libraylib.so.550 &&
            ln -sf libraylib.so.550 $build_dir/lib/libraylib.so &&
            patchelf --set-rpath '\$ORIGIN/lib' $build_dir/zeal-native &&
            mv $build_dir/zeal-native $build_dir/$binary &&
            file $build_dir/$binary &&
            file $build_dir/lib/libraylib.so.550 &&
            readelf -d $build_dir/$binary | grep NEEDED
        "
}

services=""
MESON_ARGS=""
parse_meson_args=false
for arg in "$@"; do
    if [ "$parse_meson_args" = true ]; then
        MESON_ARGS="$MESON_ARGS $arg"
    elif [ "$arg" = "--" ]; then
        parse_meson_args=true
    else
        services="$services $arg"
    fi
done

if [ -z "$services" ]; then
    services="build-rpi3 build-rpi64 build-rpi-gpi2 build-rpi0"
fi
export MESON_ARGS

failed_services=""
for service in $services; do
    echo "==> Building $service"
    if [ "$compose" = "podman-direct" ]; then
        if ! run_with_podman_direct "$service"; then
            failed_services="$failed_services $service"
        fi
    else
        # shellcheck disable=SC2086
        if ! $compose -f "$compose_file" run --rm "$service"; then
            failed_services="$failed_services $service"
        fi
    fi
done

echo "==> Done"
echo "build-rpi0/zeal-native.armv6"
echo "build-rpi3/zeal-native.armv7"
echo "build-rpi64/zeal-native.arm64"
echo "build-rpi-gpi2/zeal-native.gpi2-arm64"

if [ -n "$failed_services" ]; then
    echo "error: failed services:$failed_services" >&2
    exit 1
fi
