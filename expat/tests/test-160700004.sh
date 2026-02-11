#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

>time.txt 2>&1 /usr/bin/time -l "$scriptdir"/xmlwf \
    "$scriptdir"/clusterfuzz-testcase-minimized-xml_parse_fuzzer_UTF-16BE-5594446747205632.xml.txt

reasonable=$((128 * 1024 * 1024))
memsz=$(grep 'peak memory' time.txt | grep -Eo '[0-9]+')
if [ "$memsz" -gt "$reasonable" ]; then
	1>&2 echo "Reasonable memory limit ($reasonable) exceeded: peak $memsz"
	1>&2 echo "== time.txt"
	1>&2 cat time.txt
	exit 1
fi
