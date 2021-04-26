#!/bin/sh

file_name=`mktemp /tmp/XXXXXX`
file_ctime=`/usr/bin/stat -f%c ${file_name}`

/usr/bin/touch $file_name
file_mtime=`/usr/bin/stat -f%m ${file_name}`

if [ "$file_ctime" -gt "$file_mtime" ]; then
	echo "file's mod time ($file_mtime) should be later than the file's creation time ($file_ctime)"
	exit 1
fi

exit 0
