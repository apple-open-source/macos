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
star_prereq gnu-multi-fail-volume1.gtar
star_prereq gnu-multi-fail-volume2.gtar
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvM -f $STAR_TESTSCRIPTS/gnu-multi-fail-volume1.gtar \
               -f $STAR_TESTSCRIPTS/gnu-multi-fail-volume2.gtar

out="\
drwxrwsr-x joerg/bs          0 2003-10-11 14:32:43 OBJ/i386-sunos5-gcc/
-rw-r--r-- joerg/bs          1 2003-10-11 14:32:50 OBJ/i386-sunos5-gcc/Dnull
-rw-r--r-- joerg/bs       1743 2003-10-10 18:06:58 OBJ/i386-sunos5-gcc/star.d
-rw-r--r-- joerg/bs       1460 2003-10-11 11:53:36 OBJ/i386-sunos5-gcc/header.d
-rw-r--r-- joerg/bs       1540 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/cpiohdr.d
-rw-r--r-- joerg/bs       2245 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/xheader.d
-rw-r--r-- joerg/bs       1254 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/xattr.d
-rw-r--r-- joerg/bs       1330 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/list.d
-rw-r--r-- joerg/bs       1745 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/extract.d
-rw-r--r-- joerg/bs       1518 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/create.d
-rw-r--r-- joerg/bs       1235 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/append.d
-rw-r--r-- joerg/bs       1368 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/diff.d
-rw-r--r-- joerg/bs       1423 2003-10-10 18:06:59 OBJ/i386-sunos5-gcc/remove.d
-rw-r--r-- joerg/bs       1493 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/star_unix.d
-rw-r--r-- joerg/bs       1572 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/acl_unix.d
-rw-r--r-- joerg/bs       1453 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/fflags.d
-rw-r--r-- joerg/bs       2257 2003-10-11 14:32:43 OBJ/i386-sunos5-gcc/buffer.d
-rw-r--r-- joerg/bs        969 2003-10-07 17:53:47 OBJ/i386-sunos5-gcc/dirtime.d
-rw-r--r-- joerg/bs       1308 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/lhash.d
-rw-r--r-- joerg/bs       1287 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/hole.d
-rw-r--r-- joerg/bs       1105 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/longnames.d
-rw-r--r-- joerg/bs       1230 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/names.d
-rw-r--r-- joerg/bs       1091 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/movearch.d
-rw-r--r-- joerg/bs        961 2003-10-07 17:53:48 OBJ/i386-sunos5-gcc/table.d
-rw-r--r-- joerg/bs       1113 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/props.d
-rw-r--r-- joerg/bs       2146 2003-10-10 18:07:00 OBJ/i386-sunos5-gcc/fetchdir.d
-rw-r--r-- joerg/bs       1093 2003-10-10 18:07:01 OBJ/i386-sunos5-gcc/unicode.d
-rw-r--r-- joerg/bs       1211 2003-10-10 18:07:01 OBJ/i386-sunos5-gcc/subst.d
-rw-r--r-- joerg/bs       2076 2003-10-11 11:53:36 OBJ/i386-sunos5-gcc/volhdr.d
-rw-r--r-- joerg/bs       1480 2003-10-10 18:07:01 OBJ/i386-sunos5-gcc/chdir.d
-rw-r--r-- joerg/bs      42460 2003-10-10 18:07:02 OBJ/i386-sunos5-gcc/star.o
-rw-r--r-- joerg/bs      22564 2003-10-11 11:53:37 OBJ/i386-sunos5-gcc/header.o
-rw-r--r-- joerg/bs       7880 2003-10-10 18:07:04 OBJ/i386-sunos5-gcc/cpiohdr.o
-rw-r--r-- joerg/bs      14624 2003-10-10 18:07:05 OBJ/i386-sunos5-gcc/xheader.o
-rw-r--r-- joerg/bs        924 2003-10-10 18:07:05 OBJ/i386-sunos5-gcc/xattr.o
-rw-r--r-- joerg/bs       6120 2003-10-10 18:07:05 OBJ/i386-sunos5-gcc/list.o
-rw-r--r-- joerg/bs      12764 2003-10-10 18:07:06 OBJ/i386-sunos5-gcc/extract.o
-rw-r--r-- joerg/bs      14668 2003-10-10 18:07:06 OBJ/i386-sunos5-gcc/create.o
-rw-r--r-- joerg/bs       2576 2003-10-10 18:07:07 OBJ/i386-sunos5-gcc/append.o
-rw-r--r-- joerg/bs       7636 2003-10-10 18:07:07 OBJ/i386-sunos5-gcc/diff.o
-rw-r--r-- joerg/bs       3072 2003-10-10 18:07:07 OBJ/i386-sunos5-gcc/remove.o
-rw-r--r-- joerg/bs       5612 2003-10-10 18:07:08 OBJ/i386-sunos5-gcc/star_unix.o
-rw-r--r-- joerg/bs       6220 2003-10-10 18:07:08 OBJ/i386-sunos5-gcc/acl_unix.o
-rw-r--r-- joerg/bs       1092 2003-10-10 18:07:08 OBJ/i386-sunos5-gcc/fflags.o
-rw-r--r-- joerg/bs      20996 2003-10-11 14:32:44 OBJ/i386-sunos5-gcc/buffer.o
-rw-r--r-- joerg/bs       2060 2003-10-07 17:53:57 OBJ/i386-sunos5-gcc/dirtime.o
-rw-r--r-- joerg/bs       1664 2003-10-10 18:07:09 OBJ/i386-sunos5-gcc/lhash.o
-rw-r--r-- joerg/bs      10564 2003-10-10 18:07:10 OBJ/i386-sunos5-gcc/hole.o
-rw-r--r-- joerg/bs       3864 2003-10-10 18:07:10 OBJ/i386-sunos5-gcc/longnames.o
-rw-r--r-- joerg/bs       2576 2003-10-10 18:07:10 OBJ/i386-sunos5-gcc/names.o
-rw-r--r-- joerg/bs        952 2003-10-10 18:07:10 OBJ/i386-sunos5-gcc/movearch.o
-rw-r--r-- joerg/bs       2756 2003-10-07 17:53:59 OBJ/i386-sunos5-gcc/table.o
"

