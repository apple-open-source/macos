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
star_prereq ustar-big-2g.tar.bz2
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvjf $STAR_TESTSCRIPTS/ustar-big-2g.tar.bz2

out="\
-rw------- jes/glone 2147483647 2002-06-15 14:53:23 big
-rw-r--r-- jes/glone          0 2002-06-15 14:53:32 file
"

err_regex="\
tar: Record size = .*
"

. $srcdir/after
