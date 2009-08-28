#!/bin/sh

make distclean

for configscript in `find . -name configure`
do
  rm -f $configscript
done