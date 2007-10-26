#! /bin/bash

BASEDIR=$(basename $(cd $(dirname $0)/.. ; pwd))
BUILDIT_DIR=${BUILDIT_DIR:-/tmp}
SVNROOT=$(svn info | awk '/URL/{print $2}')
SVNREV=$(svn info | awk '/Revision/{print $2}')
PROJECT=samba


[ -d $BUILDIT_DIR/$BASEDIR.roots ] && ROOTS=$BUILDIT_DIR/$BASEDIR.roots
[ -d $BUILDIT_DIR/$BASEDIR~roots ] && ROOTS=$BUILDIT_DIR/$BASEDIR~roots

if [ "$ROOTS" = "" ] ; then
    echo Failed to guess buildit ROOTS directory
    exit 1
fi

[ -d $ROOTS/$BASEDIR.dst ] && DSTROOT=$ROOTS/$BASEDIR.dst
[ -d $ROOTS/$BASEDIR~dst ] && DSTROOT=$ROOTS/$BASEDIR~dst
[ -d $ROOTS/$BASEDIR.sym ] && SYMROOT=$ROOTS/$BASEDIR.sym
[ -d $ROOTS/$BASEDIR~sym ] && SYMROOT=$ROOTS/$BASEDIR~sym

echo SYMROOT=$SYMROOT
echo DSTROOT=$DSTROOT

[ -d "$SYMROOT" ] && \
    (cd $SYMROOT && tar -cvf - *) | gzip -c > samba-$BASEDIR-r$SVNREV-sym.tgz

[ -d "$DSTROOT" ] && \
    (cd $DSTROOT && tar -cvf - *) | gzip -c > samba-$BASEDIR-r$SVNREV-dst.tgz

