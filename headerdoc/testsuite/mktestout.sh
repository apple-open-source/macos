#!/bin/sh

rm -rf test_output
mkdir test_output
for i in sources/* ; do
	echo "PROCESSING $i"
	../headerDoc2HTML.pl -o test_output $i
	../headerDoc2HTML.pl -X -o test_output $i
done

