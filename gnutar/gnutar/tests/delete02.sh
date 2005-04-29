#! /bin/sh

# Deleting a member with the archive from stdin was not working correctly.

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
genfile -l 3073 -p zeros > 1
cp 1 2
cp 2 3
tar cf archive 1 2 3
tar tf archive
cat archive | tar f - --delete 2 > archive2
echo -----
tar tf archive2

out="\
1
2
3
-----
1
3
"

. $srcdir/after
