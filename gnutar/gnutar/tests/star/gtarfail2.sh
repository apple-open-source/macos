#! /bin/sh
# This file is part of GNU tar testsuite.
# Copyright (C) 2004 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

. ./preset
star_prereq gtarfail2.tar
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvf $STAR_TESTSCRIPTS/gtarfail2.tar

out="\
-rwxr-xr-x jes/glone       214 2001-09-21 14:08:36 .clean
lrwxrwxrwx jes/cats          0 1998-05-07 12:39:00 RULES -> makefiles/RULES
drwxr-sr-x jes/glone         0 2001-12-10 00:00:58 build/
-rw-r--r-- jes/glone    312019 2001-12-10 00:00:20 build/smake-1.2.tar.gz
drwxr-sr-x jes/glone         0 2001-11-09 18:20:33 build/psmake/
-rwxr-xr-x jes/glone       259 2000-01-09 16:36:34 build/psmake/MAKE
-rwxr-xr-x jes/glone      4820 2001-02-25 22:45:53 build/psmake/MAKE.sh
-rw-r--r-- jes/glone       647 2001-02-25 23:50:06 build/psmake/Makefile
lrwxrwxrwx jes/glone         0 2001-08-29 10:53:53 build/psmake/archconf.c -> ../archconf.c
lrwxrwxrwx jes/glone         0 2001-08-29 10:54:00 build/psmake/astoi.c -> ../../lib/astoi.c
"

. $srcdir/after
