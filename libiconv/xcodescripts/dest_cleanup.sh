set -x

DSTROOT="${DSTROOT}${INSTALL_PATH_PREFIX}"

if [[ ${UID} -eq 0 ]]; then
	chown -hR root:wheel ${DSTROOT}/usr
fi

ln -s libiconv.2.4.0.dylib   ${DSTROOT}/usr/lib/libiconv.dylib
ln -s libiconv.2.dylib       ${DSTROOT}/usr/lib/libiconv.2.4.0.dylib
ln -s libcharset.1.0.0.dylib ${DSTROOT}/usr/lib/libcharset.dylib
ln -s libcharset.1.dylib     ${DSTROOT}/usr/lib/libcharset.1.0.0.dylib

find ${DSTROOT}/usr -type f -perm +u+w -print0 | xargs -t -0 chmod -h u-w
