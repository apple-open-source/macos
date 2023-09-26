#!/bin/sh

cd ../bugs-fixed

echo === $1
OUT=${1%.awk}.OUT
OK=${1%.awk}.ok
IN=${1%.awk}.in
input=
if [ -f $IN ]
then
    input=$IN
fi

../a.out -f $1 $input > $OUT 2>&1
if cmp -s $OK $OUT
then
    rm -f $OUT
else
    echo ++++ $1 failed!
fi
