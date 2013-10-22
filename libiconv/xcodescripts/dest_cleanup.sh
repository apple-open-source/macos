set -x

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1

	mv ${DSTROOT}/usr/lib/charset.alias ${DSTROOT}${SDKROOT}/usr/lib
	mv ${DSTROOT}/usr/local ${DSTROOT}${SDKROOT}/usr

	[ -d "${DSTROOT}/usr" ] && rm -rf "${DSTROOT}/usr"

	DSTROOT="${DSTROOT}${SDKROOT}"
	libs="libiconv.2.dylib libcharset.1.dylib"

	for lib in ${libs} ; do
		install_name_tool -id /usr/lib/${lib} ${DSTROOT}/usr/lib/${lib}
		for lib2 in ${libs} ; do
			install_name_tool -change ${SDKROOT}/usr/lib/${lib} /usr/lib/${lib} ${DSTROOT}/usr/lib/${lib2}
		done
	done
fi

if [[ ${UID} -eq 0 ]]; then
	chown -hR root:wheel ${DSTROOT}/usr
fi

ln -s libiconv.2.4.0.dylib   ${DSTROOT}/usr/lib/libiconv.dylib
ln -s libiconv.2.dylib       ${DSTROOT}/usr/lib/libiconv.2.4.0.dylib
ln -s libcharset.1.0.0.dylib ${DSTROOT}/usr/lib/libcharset.dylib
ln -s libcharset.1.dylib     ${DSTROOT}/usr/lib/libcharset.1.0.0.dylib

find ${DSTROOT}/usr -type f -perm +u+w -print0 | xargs -t -0 chmod -h u-w
