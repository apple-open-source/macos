#!/bin/sh

if [ $# -ne 1 ]
then
	echo "Export file name required."
	exit 1
fi
expfile="$1"
expbase=`basename $expfile`

echo ====== BUILT_PRODUCTS_DIR ${BUILT_PRODUCTS_DIR} ======
echo ====== INNER_PRODUCT_SUBPATH ${INNER_PRODUCT_SUBPATH} ======

# Don't rerun this unless the file has been relinked
if [ "${BUILT_PRODUCTS_DIR}/${expbase}.timestamp" -nt "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}" ]; then
    echo "${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH} is up to date."
    exit 0
fi
#
# Under pbbuild, install target builds the target binary directly in DSTROOT, not
# in BUILT_PRODUCTS_DIR. Normal development build results in building the
# binary in SYMROOT, which is the same as BUILT_PRODUCTS_DIR.
#
if [ -e ${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH} ]
then
	binary_file=${BUILT_PRODUCTS_DIR}/${INNER_PRODUCT_SUBPATH}
elif [ -e ${DSTROOT}/${INSTALL_PATH}/${INNER_PRODUCT_SUBPATH} ]
then
	binary_file=${DSTROOT}/${INSTALL_PATH}/${INNER_PRODUCT_SUBPATH}
else
	echo ==== Can not find linked Security binary ====
	exit 1
fi
echo "Scanning $binary_file"
ARCHS=`lipo -info "$binary_file" | awk -F : '{print $3}'`
for arch in $ARCHS
do
	echo "Generating exports file ${BUILT_PRODUCTS_DIR}/${expbase}_$arch"
	nm -gp -arch $arch "$binary_file" \
	| egrep -v '^/| U _' \
	| awk '{print $3}' \
	| egrep '_(|tf|ti|C)(|Q[2-9])8Security|_(N|ZN|ZNK|ZTTN|ZTVN)8Security|_ZN9KeySchema|^_(Sec|CSSM|Authorization)' \
	| cat - "$expfile" \
	| sort -u > "${BUILT_PRODUCTS_DIR}/${expbase}_$arch"
	echo nmedit -s "${BUILT_PRODUCTS_DIR}/${expbase}_$arch" -arch $arch "$binary_file"
	nmedit -s "${BUILT_PRODUCTS_DIR}/${expbase}_$arch" -arch $arch "$binary_file"
done
touch "${BUILT_PRODUCTS_DIR}/${expbase}.timestamp"
