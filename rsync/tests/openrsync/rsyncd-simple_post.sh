#!/bin/sh

vardest="$1"

:> "$vardest"

for var in RSYNC_MODULE_NAME RSYNC_MODULE_PATH RSYNC_HOST_ADDR RSYNC_HOST_NAME \
    RSYNC_USER_NAME RSYNC_PID RSYNC_EXIT_STATUS RSYNC_RAW_STATUS; do
	eval "_value=\$$var"
	echo "$var=$_value" >> "$vardest"
done

echo >> "$vardest"
