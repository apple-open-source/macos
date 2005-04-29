#! /bin/sh

# Check sparse file handling.

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
TAR_ARCHIVE_FORMATS="gnu oldgnu posix"
. $srcdir/before

genfile --length 1000 > begin
genfile --length 1000 > end
mksparse sparsefile 512 0 ABCD 1M EFGH 2000K IJKL
tar -c -f archive --sparse begin sparsefile end
echo separator

tar tfv archive

out_regex="\
separator
-rw-r--r-- [^ ][^ ]*  *1000 [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9] begin
-rw-r--r-- [^ ][^ ]*  *10344448[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9] sparsefile
-rw-r--r-- [^ ][^ ]*  *1000 [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9] end
"

. $srcdir/after
