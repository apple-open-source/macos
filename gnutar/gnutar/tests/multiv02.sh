#! /bin/sh

# Previous versions of tar were not able to skip a member straddling
# the multivolume archive boundary. Reported by Mads Martin Joergensen
# <mmj@suse.de>

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

genfile --length 10240 > en
genfile --length 20000 > to
genfile --length 20000 > tre
genfile --length 10240 > fire

tar -c -f A.tar -f B.tar -f C.tar -M -L 30 en to tre fire
echo separator
tar -v -x -f A.tar -f B.tar -f C.tar -M en

out="\
separator
en
"

. $srcdir/after