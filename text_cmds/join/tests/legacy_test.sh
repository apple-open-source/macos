#!/bin/sh
# $FreeBSD$

SRCDIR="$(dirname "${0}")"; export SRCDIR

m4 /AppleInternal/Tests/text_cmds/regress.m4 "${SRCDIR}/regress.sh" | sh
