#! /bin/sh

# Check for fnmatch problems in glibc 2.1.95.

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
. $srcdir/before

set -e
touch file1
mkdir directory
mkdir directory/subdirectory
touch directory/file1
touch directory/file2
touch directory/subdirectory/file1
touch directory/subdirectory/file2
tar -cf archive ./file1 directory
tar -tf archive \
  --exclude='./*1' \
  --exclude='d*/*1' \
  --exclude='d*/s*/*2' | sort

out="\
directory/
directory/file2
directory/subdirectory/
"

. $srcdir/after
