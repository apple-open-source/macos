#!/bin/sh

version=$1
shift

rm -rf /tmp/$version

git archive --format=tar --prefix=$version/ $version | tar Cxf /tmp -

cd /tmp/$version

~rc/bin/submitproject . -version $version "$@"
