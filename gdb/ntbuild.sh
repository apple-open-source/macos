#! /bin/sh

TMPDIR='c:\temp'; export TMPDIR

SHELL='c:/apple/library/executables/sh.exe'; export SHELL
CONFIG_SHELL='c:/apple/library/executables/sh.exe'; export CONFIG_SHELL

make \
    SHELL='c:/apple/library/executables/sh.exe' \
    CONFIG_SHELL='c:/apple/library/executables/sh.exe' \
    YACC=bison \
    YFLAGS=-y \
    AWK=gawk \
    CC=gcc \
    PICFLAG= \
    $*
