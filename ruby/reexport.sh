#!/bin/sh

tmp="$(mktemp -d "${OBJROOT}/lipo.XXXXXX")"
output="$1"
input="$1"
input_base="$(basename ${input})"
exportlib="$2"
compat_version="$3"
current_version="$4"

for arch in ${RC_ARCHS}; do
    mkdir -p "$tmp/${arch}" || exit 1
    lipo -thin "${arch}" "${input}" -output "${tmp}/${arch}/${input_base}" || exit 1
    nm -gjU "${tmp}/${arch}/${input_base}" > "${tmp}/${arch}/exports" || exit 1
    clang -arch "${arch}" -install_name "/usr/lib/${input_base}" -shared -o "${tmp}/${arch}/${input_base}" \
	-Xlinker -reexport_library -Xlinker "${exportlib}" \
	-Xlinker -exported_symbols_list -Xlinker "${tmp}/${arch}/exports" \
	-Xlinker -macosx_version_min -Xlinker "${MACOSX_DEPLOYMENT_TARGET}" \
	-Xlinker -compatibility_version -Xlinker "${compat_version}" \
	-Xlinker -current_version -Xlinker "${current_version}" || exit 1
done

lipo -create -output "$output" "${tmp}"/*/*.dylib
codesign -f -s - "$output"
rm -rf "${tmp}"
