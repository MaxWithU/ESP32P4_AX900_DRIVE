#!/bin/sh
# Use the mbedTLS source bundled with ESP-IDF 5.5.2 (or compatible 3.6.x).
set -eu
: "${MBEDTLS_SOURCE:?Set MBEDTLS_SOURCE to the mbedTLS source directory}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ax900-tls-test.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
build_root=${MBEDTLS_HOST_BUILD:-$test_dir/build}
for cert in server other; do
    openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj /CN=radius.test \
        -addext subjectAltName=DNS:radius.test -keyout "$test_dir/$cert.key" \
        -out "$test_dir/$cert.pem" > /dev/null 2>&1
done
openssl req -new -key "$test_dir/server.key" -subj /CN=radius.test -out "$test_dir/leaf.csr" > /dev/null 2>&1
touch "$test_dir/index"
printf 'unique_subject = no\n' > "$test_dir/index.attr"
printf '01\n' > "$test_dir/serial"
cat > "$test_dir/ca.cnf" <<EOF
[ca]
default_ca=issuer
[issuer]
database=$test_dir/index
serial=$test_dir/serial
new_certs_dir=$test_dir
certificate=$test_dir/server.pem
private_key=$test_dir/server.key
default_md=sha256
policy=names
x509_extensions=server
[names]
commonName=supplied
[server]
subjectAltName=DNS:radius.test
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF
openssl ca -batch -notext -config "$test_dir/ca.cnf" -in "$test_dir/leaf.csr" \
    -startdate 20000101000000Z -enddate 20010101000000Z -out "$test_dir/expired.pem" > /dev/null 2>&1
openssl ca -batch -notext -config "$test_dir/ca.cnf" -in "$test_dir/leaf.csr" \
    -startdate 20990101000000Z -enddate 20991231000000Z -out "$test_dir/future.pem" > /dev/null 2>&1
for mode in date no-date; do
    build_dir=$build_root/tls-$mode
    mkdir -p "$build_dir"
    config=$build_dir/ax900-test-config.h
    printf '#define MBEDTLS_PLATFORM_TIME_ALT\n' > "$config"
    if [ "$mode" = no-date ]; then printf '#undef MBEDTLS_HAVE_TIME_DATE\n' >> "$config"; fi
    cmake -S "$MBEDTLS_SOURCE" -B "$build_dir" -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF \
        -DMBEDTLS_FATAL_WARNINGS=OFF -DCMAKE_BUILD_TYPE=Debug -DMBEDTLS_USER_CONFIG_FILE="$config" \
        > "$test_dir/configure.log" 2>&1 || { cat "$test_dir/configure.log"; exit 1; }
    cmake --build "$build_dir" -j 8 > "$test_dir/build.log" 2>&1 || { tail -50 "$test_dir/build.log"; exit 1; }
    cc -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
        -DCONFIG_NO_STDOUT_DEBUG -DCONFIG_NO_WPA_MSG -DMBEDTLS_USER_CONFIG_FILE=\"$config\" \
        -I "$repo/tests/host" -I "$repo/components/ax900/supplicant/src" \
        -I "$repo/components/ax900/supplicant/src/utils" -I "$MBEDTLS_SOURCE/include" \
        "$repo/tests/tls_bridge_test.c" "$repo/components/ax900/ax900_tls.c" \
        "$repo/components/ax900/supplicant/src/utils/wpabuf.c" \
        "$build_dir/library/libmbedtls.a" "$build_dir/library/libmbedx509.a" \
        "$build_dir/library/libmbedcrypto.a" -o "$test_dir/test"
    "$test_dir/test" "$test_dir/server.pem" "$test_dir/server.key" "$test_dir/other.pem" "$test_dir/expired.pem" "$test_dir/future.pem"
done
