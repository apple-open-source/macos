#!/bin/sh

framework="$1"

found=no

for a in ${TEST_FRAMEWORK_SEARCH_PATHS}; do
    if test -d "${a}/${framework}" ; then
        dst="${BUILT_PRODUCTS_DIR}/${FRAMEWORKS_FOLDER_PATH}"

	    mkdir -p "${dst}" || { echo "mkdir failed with: $?" ; exit 1; }
        ditto  "${a}/${framework}" "${dst}/${framework}" || { echo "ditto failed with: $?" ; exit 1; }
        find "${dst}/${framework}" \( -name Headers -o -name Modules -o -name '*.tbd' \) -delete
        xcrun codesign -s - -f "${dst}/${framework}" || { echo "codesign failed with: $?" ; exit 1; }

        found=yes
	    break
    fi
done

test "X${found}" != "Xyes" && exit 1

exit 0
