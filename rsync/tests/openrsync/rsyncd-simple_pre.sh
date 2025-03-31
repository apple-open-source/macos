#!/bin/sh

vardest="$1"

:> "$vardest"

for var in RSYNC_MODULE_NAME RSYNC_MODULE_PATH RSYNC_HOST_ADDR RSYNC_HOST_NAME \
    RSYNC_USER_NAME RSYNC_PID RSYNC_REQUEST; do
	eval "_value=\$$var"
	echo "$var=$_value" >> "$vardest"
done

printf "RSYNC_ARGS=" >> "$vardest"
argno=0
while true; do
	eval "_arg=\$RSYNC_ARG$argno"

	if [ "$argno" -ne 0 ]; then
		printf " " >> "$vardest"
	fi

	printf "%s" "$_arg" >> "$vardest"

	if [ "$_arg" = "." ]; then
		break
	fi
	argno=$((argno + 1))
done

echo >> "$vardest"
