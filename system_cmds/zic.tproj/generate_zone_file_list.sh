#!/bin/sh

# we need to know where the data files are...
if [ $# -ne 1 ]; then
    echo "Usage: $0 DATA_FILES_DIR" 1>&2
    exit 1
fi

DATFILES="$1"
ZONE_FILES="$(egrep --files-with-match '^(Zone|Rule|Link)' ${DATFILES}/* | awk -F "/" '{print $NF}')"

for tz in ${ZONE_FILES}; do
    if [ ${tz} = "backward" ]; then
        DO_BACKWARD=1
        continue
    elif [ ${tz} = "backzone" ]; then
        continue
    else
        echo "${tz}"
    fi
done

if [ -n "$DO_BACKWARD" ]; then
    echo "backward"
fi

