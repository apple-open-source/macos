#! /bin/sh
# Old format (V7) archives should not accept file names longer than
# 99 characters

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
TAR_ARCHIVE_FORMATS="v7" 
. $srcdir/before

DIR=this_is_a_very_long_name_for_a_directory_which_causes_problems
FILE=this_is_a_very_long_file_name_which_raises_issues.c
mkdir $DIR
touch $DIR/$FILE

tar cf archive $DIR
echo separator
tar tf archive

err="\
tar: $DIR/$FILE: file name is too long (max 99); not dumped
tar: Error exit delayed from previous errors
"

out="\
separator
$DIR/
"

. $srcdir/after

