#!/bin/sh
# Use the mbedTLS source bundled with ESP-IDF 5.5.2 (or compatible 3.6.x).
set -eu
: "${MBEDTLS_SOURCE:?Set MBEDTLS_SOURCE to the mbedTLS source directory}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ax900-tls-test.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
build_dir=${MBEDTLS_HOST_BUILD:-$test_dir/build}
cmake -S "$MBEDTLS_SOURCE" -B "$build_dir" -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF \
    -DMBEDTLS_FATAL_WARNINGS=OFF -DCMAKE_BUILD_TYPE=Debug > "$test_dir/configure.log" 2>&1 || { cat "$test_dir/configure.log"; exit 1; }
cmake --build "$build_dir" -j 8 > "$test_dir/build.log" 2>&1 || { tail -50 "$test_dir/build.log"; exit 1; }
for cert in server other; do
    openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj /CN=radius.test \
        -addext subjectAltName=DNS:radius.test -keyout "$test_dir/$cert.key" \
        -out "$test_dir/$cert.pem" > /dev/null 2>&1
done
cc -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
    -DCONFIG_NO_STDOUT_DEBUG -DCONFIG_NO_WPA_MSG \
    -I "$repo/tests/host" -I "$repo/components/ax900/supplicant/src" \
    -I "$repo/components/ax900/supplicant/src/utils" -I "$MBEDTLS_SOURCE/include" \
    "$repo/tests/tls_bridge_test.c" "$repo/components/ax900/ax900_tls.c" \
    "$repo/components/ax900/supplicant/src/utils/wpabuf.c" \
    "$build_dir/library/libmbedtls.a" "$build_dir/library/libmbedx509.a" \
    "$build_dir/library/libmbedcrypto.a" -o "$test_dir/test"
"$test_dir/test" "$test_dir/server.pem" "$test_dir/server.key" "$test_dir/other.pem"
