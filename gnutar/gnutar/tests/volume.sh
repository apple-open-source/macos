#! /bin/sh

# Volume labels are checked on read by fnmatch.

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

tar -cf archive -V label -T /dev/null || exit 1

tar xfV archive label || exit 1
tar xfV archive 'la?el' || exit 1
tar xfV archive 'l*l' || exit 1

echo 1>&2 -----
tar xfV archive lab
test $? = 2 || exit 1
echo 1>&2 -----
tar xfV archive bel
test $? = 2 || exit 1
echo 1>&2 -----
tar xfV archive babel
test $? = 2 || exit 1

err="\
-----
tar: Volume \`label' does not match \`lab'
tar: Error is not recoverable: exiting now
-----
tar: Volume \`label' does not match \`bel'
tar: Error is not recoverable: exiting now
-----
tar: Volume \`label' does not match \`babel'
tar: Error is not recoverable: exiting now
"

. $srcdir/after
