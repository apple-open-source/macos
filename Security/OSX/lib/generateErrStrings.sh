#/bin/bash

set -x

# make error message string files

GENDEBUGSTRS[0]=YES; ERRORSTRINGS[0]="${DERIVED_SRC}/SecDebugErrorMessages.strings"
GENDEBUGSTRS[1]=NO ; ERRORSTRINGS[1]="${DERIVED_SRC}/en.lproj/SecErrorMessages.strings"

mkdir -p "${DERIVED_SRC}/en.lproj"

for ((ix=0;ix<2;ix++)) ; do
perl OSX/lib/generateErrStrings.pl \
${GENDEBUGSTRS[ix]} \
"${DERIVED_SRC}" \
"${ERRORSTRINGS[ix]}" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/Authorization.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/AuthSession.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/SecureTransport.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/SecBase.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/cssmerr.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/cssmapple.h" \
"${BUILT_PRODUCTS_DIR}/Security.framework/Headers/CSCommon.h" \
"${PROJECT_DIR}/OSX/libsecurity_keychain/lib/MacOSErrorStrings.h"
done
