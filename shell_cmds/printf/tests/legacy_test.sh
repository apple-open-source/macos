#!/bin/sh
# $FreeBSD$

SRCDIR="$(dirname "${0}")"; export SRCDIR

m4 /AppleInternal/Tests/shell_cmds/regress.m4 "${SRCDIR}/regress.sh" | sh
