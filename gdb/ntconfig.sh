#! /bin/sh

NTCONFIG=$0
TOPDIR=`dirname $NTCONFIG`
TOPDIR=`cd ${TOPDIR}; pwd`
TOPDRIVE=`echo ${TOPDIR} | sed -e s/:.*//`
TOPDIR=`echo ${TOPDIR} | sed -e s/.*://`

CURDIR=`pwd`
CURDRIVE=`echo ${CURDIR} | sed -e s/:.*//`
CURDIR=`echo ${CURDIR} | sed -e s/.*://`

TMPDIR='c:\temp'; export TMPDIR
CONFIG_SHELL='c:/apple/library/executables/sh.exe'; export CONFIG_SHELL
EXE='.exe'; export EXE

SHELL='c:/apple/library/executables/sh.exe'; export SHELL
LD='c:/apple/developer/executables/link.exe'; export SHELL
CC='gcc'; export CC

if [ ${CURDRIVE} != ${TOPDRIVE} ]; then
    echo "error: build tree must be located on the same drive letter as source (${TOPDRIVE} vs. ${CURDRIVE})."
    exit 1
fi

echo "Configuring GDB using ${TOPDIR}/gdb/configure"
${TOPDIR}/gdb/configure --host=i386-nextpdo-winnt3.5 -v $*
