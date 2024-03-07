#!/bin/sh
#
# Copyright (c) 2003 Poul-Henning Kamp
# All rights reserved.
#
# Please see src/share/examples/etc/bsd-style-copyright.
#
# $FreeBSD: src/usr.sbin/mtree/test/test03.sh,v 1.2 2005/03/29 11:44:17 tobez Exp $
#

set -e

TMP=/tmp/mtree.$$

rm -rf ${TMP}
mkdir -p ${TMP}

K=uid,uname,gid,gname,flags,md5digest,size,ripemd160digest,sha1digest,sha256digest,cksum

mkdir ${TMP}/_FOO
touch ${TMP}/_FOO/_uid
touch ${TMP}/_FOO/_size
touch ${TMP}/_FOO/zztype
touch ${TMP}/_FOO/_bar
mtree -c -K $K -p ${TMP}/_FOO > ${TMP}/_r
mtree -c -K $K -p ${TMP}/_FOO > ${TMP}/_r2
rm -rf ${TMP}/_FOO/_bar

rm -rf ${TMP}/_FOO/zztype
mkdir ${TMP}/_FOO/zztype

date > ${TMP}/_FOO/_size

touch ${TMP}/_FOO/_foo
mtree -c -K $K -p ${TMP}/_FOO > ${TMP}/_t

rm -rf ${TMP}/_FOO

if mtree -f ${TMP}/_r -f ${TMP}/_r2 ; then
	true
else
	echo "ERROR Compare identical failed" 1>&2
	exit 1
fi
	
if mtree -f ${TMP}/_r -f ${TMP}/_t > ${TMP}/_ ; then
	echo "ERROR Compare different succeeded" 1>&2
	exit 1
fi

if [ `wc -l  < ${TMP}/_` -ne 8 ] ; then
	echo "ERROR wrong number of lines: `wc -l  ${TMP}/_`" 1>&2
	exit 1
fi
	
exit 0
