#!/bin/bash -e -x

if [[ ${UID} -eq 0 ]]; then
	chown -hR root:wheel ${DSTROOT}/usr
fi

if [[ "${ACTION}" == "install" ]]; then
  ln -fs libiconv.2.dylib   ${DSTROOT}/usr/lib/libiconv.dylib
  ln -fs libcharset.1.dylib ${DSTROOT}/usr/lib/libcharset.dylib
fi

if [[ "${ACTION}" == "install" || "${ACTION}" == "installapi" ]]; then
  ln -fs libiconv.2.tbd   ${DSTROOT}/usr/lib/libiconv.tbd
  ln -fs libcharset.1.tbd ${DSTROOT}/usr/lib/libcharset.tbd
fi

find ${DSTROOT}/usr -type f -perm +u+w -print0 | xargs -t -0 chmod -h u-w
