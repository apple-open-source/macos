#!/bin/sh
# $FreeBSD$

SRCDIR="$(dirname "${0}")"; export SRCDIR

#ifdef __APPLE__
# We create these files here rather than letting them get installed to avoid the
# need for a verifier exception (they are empty).
:> ${SRCDIR}/regress.wdl.out
:> ${SRCDIR}/regress.wxn.out
#endif

m4 /AppleInternal/Tests/shell_cmds/regress.m4 "${SRCDIR}/regress.sh" | sh
