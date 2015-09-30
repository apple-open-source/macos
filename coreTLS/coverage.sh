#!/bin/bash

BUILDDIR=$1
ARCH="x86_64"
VERSION=`git describe`

echo "##### BUILDIR=$BUILDIR, ARCH=$ARCH, VERSION=$VERSION"

find $BUILDDIR -path "*/Objects-coverage/$ARCH/*.gcda" | xargs rm

echo "##### Step 1: Run tls_test_coverage..."

$BUILDDIR/Build/Products/Debug/tls_test_coverage
$BUILDDIR/Build/Products/Debug/tls_test_coverage tls_03_client


echo "##### Step 2: Gather coverage data..."

OUTPUTDIR=/tmp/coretls-coverage-$VERSION/

mkdir $OUTPUTDIR
cd $OUTPUTDIR

find $BUILDDIR -path "*/Objects-coverage/$ARCH/*.o" | xargs gcov


echo Coverage data gathered in $PWD.


