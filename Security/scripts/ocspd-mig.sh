#!/bin/sh

mkdir -p "${DERIVED_SRC}"

xcrun mig -isysroot "${SDKROOT}" \
-server "${DERIVED_SRC}/security_ocspd/ocspd_server.cpp" \
-user "${DERIVED_SRC}/security_ocspd/ocspd_client.cpp" \
-header "${DERIVED_SRC}/security_ocspd/ocspd.h" \
"${PROJECT_DIR}/OSX/libsecurity_ocspd/mig/ocspd.defs"
