#! /bin/sh

# Check if listed-incremental backups work for individual files.
# Script proposed by Andreas Schuldei <andreas@schuldei.org>

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
TAR_ARCHIVE_FORMATS="gnu oldgnu" 
. $srcdir/before

mkdir directory
genfile --length 10240 --pattern zeros > directory/file1
# Let the things settle
sleep 1

tar --create \
    --file=archive.1 \
    --listed-incremental=listing \
    directory/file*

tar tf archive.1

dd if=/dev/zero of=directory/file2 bs=1024 count=20 2>/dev/null

echo "separator"

tar --create \
    --file=archive.2 \
    --listed-incremental=listing \
    directory/file*

tar tf archive.2

out="\
directory/file1
separator
directory/file2
"

. $srcdir/after
