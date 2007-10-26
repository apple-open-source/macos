#! /bin/sh

DEPS=${DEPS:-y}
target="$@"

# Default target is build
[ -z "$target" ] && target=build

case $DEPS in
    n|no|N|NO) target=NODEPS$target ;;
esac

cd $(dirname $0)/..
exec make -f Makefile.local $target
