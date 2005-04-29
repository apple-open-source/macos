#! /bin/sh

# In previous versions a single -C option did not work with --same-order
# (see same-order01.sh). However, multiple -C options worked OK. 
# Test if we did not break the correct behavior.

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
genfile -l 1024 > file1
genfile -l 1024 > file2
tar cf archive file1 file2

mkdir en
mkdir to

HERE=`pwd`
tar -xf archive --same-order -C $HERE/en file1 -C $HERE/to file2

ls en
echo separator
ls to

out="\
file1
separator
file2
"

. $srcdir/after
