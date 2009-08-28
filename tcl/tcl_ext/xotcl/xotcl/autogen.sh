#!/bin/sh

for pdir in `find . -name configure.in`
do
  (cd `dirname $pdir`; autoconf)
done
