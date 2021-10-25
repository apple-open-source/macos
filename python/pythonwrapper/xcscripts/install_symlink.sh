#!/bin/sh
_src="${INSTALL_PATH}/${EXECUTABLE_PATH}"
_dir="${DSTROOT}${INSTALL_PATH}/../bin"
_dst="${_dir}/${PRODUCT_NAME}"
mkdir -p "${_dir}"
ln -sf "${_src}" "${_dst}"

