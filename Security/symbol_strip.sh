#!/bin/sh

if [ $# -ne 1 ]
then
	echo "Export file name required."
	exit 1
fi
expfile="$1"
expbase=`basename $expfile`

# Don't rerun this unless the file has been relinked
if [ "${BUILT_PRODUCTS_DIR}/${expbase}.timestamp" -nt "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}" ]; then
    echo "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH} is up to date."
    exit 0
fi

echo "Scanning ${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}"
ARCHS=`lipo -info "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}" | awk -F : '{print $3}'`
for arch in $ARCHS
do
	echo "Generating exports file ${BUILT_PRODUCTS_DIR}/${expbase}_$arch"
	nm -gp -arch $arch "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}" \
	| egrep -v '^/| U _' \
	| awk '{print $3}' \
	| egrep '_(|tf|ti|C)(|Q[2-9])8Security|_(N|ZN|ZNK|ZTTN|ZTVN)8Security|^_(Sec|CSSM|Authorization)' \
	| cat - "$expfile" \
	| sort -u > "${BUILT_PRODUCTS_DIR}/${expbase}_$arch"
	echo nmedit -s "${BUILT_PRODUCTS_DIR}/${expbase}_$arch" -arch $arch "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}"
	nmedit -s "${BUILT_PRODUCTS_DIR}/${expbase}_$arch" -arch $arch "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}"
done
touch "${BUILT_PRODUCTS_DIR}/${expbase}.timestamp"
