#!/bin/sh

# If building for embedded, just punt
expr "${CONFIGURATION}" : ".*Embedded.*" > /dev/null && exit 0

base="$1"

#
# Re-add missing symlink headers if the Headers/PrivateHeaders is present
#
for a in "Headers" "PrivateHeaders" ; do
    if [ -d "${base}/Versions/Current/${a}" ] ; then
	rm -f "${base}/${a}"
	ln -s "Versions/Current/${a}" "${base}/${a}" || exit 1
    fi
done

exit 0
