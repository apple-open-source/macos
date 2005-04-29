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
star_prereq ustar-all-quicktest.tar
star_prereq quicktest.filelist
# Only root may perform this test
test -w / || skiptest

TAR_ARCHIVE_FORMATS=ustar
. $srcdir/before

mkdir directory
cd directory

save_TAR_OPTIONS=$TAR_OPTIONS
TAR_OPTIONS="" tar xf $STAR_TESTSCRIPTS/ustar-all-quicktest.tar
TAR_OPTIONS=$save_TAR_OPTIONS
echo separator
echo separator >&2
tar cfT ../archive $STAR_TESTSCRIPTS/quicktest.filelist
cd ..

${TARTEST:-tartest} -v < $STAR_TESTSCRIPTS/ustar-all-quicktest.tar > old.out
${TARTEST:-tartest} -v < archive > new.out

cmp old.out new.out

out="\
separator
"

err_ignore="tar: Extracting contiguous files as regular files"

err="\
separator
"
					
. $srcdir/after
