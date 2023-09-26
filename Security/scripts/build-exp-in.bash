#!/bin/bash -x

USE_ARCH=""
FILENAME_ARCH=""
if [ -n "$1" ] ; then
  USE_ARCH="-arch ${1}"
  FILENAME_ARCH=".${1}"
fi

# This is a bit of a hack.
# The HEADER_SEARCH_PATHS env var is a single string containing
# directory names separated by spaces. But if any of those names
# contain a space, how do we know that space is not a separator?
#
# So we assume each separate directory name starts with a '/',
# and iterate over all space-separated tokens, accruing until
# we find a leading '/'. We put each directory name into an
# array entry, so we can use "${HEADER_SEARCH_OPTIONS[@]}"
# in the invocation of clang below, so that each entry in the
# array is replaced with a quoted string.
unset HEADER_SEARCH_OPTIONS
j=1
acc=""
for i in $HEADER_SEARCH_PATHS ; do
  if [ $(echo ${i} | cut -c1) == "/" ] ; then
    if [ -n "${acc}" ] ; then
      HEADER_SEARCH_OPTIONS[$j]="-I${acc}"
      j=$((j+1))
    fi
    acc="${i}"
  else
    acc="${acc} ${i}"
  fi
done
HEADER_SEARCH_OPTIONS[$j]="-I${acc}"

for prep in ${GCC_PREPROCESSOR_DEFINITIONS[@]} ; do
PREPROCESSOR="${PREPROCESSOR} -D${prep}"
done

xcrun clang -E -Xpreprocessor -P -x objective-c ${USE_ARCH} "${HEADER_SEARCH_OPTIONS[@]}" ${OTHER_INPUT_FILE_FLAGS} ${PREPROCESSOR} "${INPUT_FILE_PATH}" -o "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}${FILENAME_ARCH}.exp"
