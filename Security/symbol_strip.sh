#!/bin/sh

if [ $# -ne 1 ]
then
	echo "Export file name required."
	exit 1
fi
expfile="$1"
expbase=`basename $expfile`

# Don't rerun this unless the file has been relinked
if [ "${SYMROOT}/${expbase}.timestamp" -nt "${SYMROOT}/${INNER_PRODUCT_SUBPATH}" ]; then
    echo "${SYMROOT}/${INNER_PRODUCT_SUBPATH} is up to date."
    exit 0
fi

echo "Scanning ${SYMROOT}/${INNER_PRODUCT_SUBPATH}"
ARCHS=`lipo -info "${SYMROOT}/${INNER_PRODUCT_SUBPATH}" | awk -F : '{print $3}'`
for arch in $ARCHS
do
	echo "Generating exports file ${SYMROOT}/${expbase}_$arch"
	nm -gp -arch $arch "${SYMROOT}/${INNER_PRODUCT_SUBPATH}" \
	| egrep -v '^/| U _' \
	| awk '{print $3}' \
	| egrep '_(|tf|ti|C)(|Q[2-9])8Security|^_(Sec|CSSM|Authorization)' \
	| cat - "$expfile" \
	| sort -u > "${SYMROOT}/${expbase}_$arch"
	echo nmedit -s "${SYMROOT}/${expbase}_$arch" -arch $arch "${SYMROOT}/${INNER_PRODUCT_SUBPATH}"
	nmedit -s "${SYMROOT}/${expbase}_$arch" -arch $arch "${SYMROOT}/${INNER_PRODUCT_SUBPATH}"
done
touch "${SYMROOT}/${expbase}.timestamp"
