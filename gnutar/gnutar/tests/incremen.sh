#! /bin/sh

# A directory older than the listed entry was skipped completely.

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

set -e
mkdir structure
echo x >structure/file

# On Nextstep (and perhaps other 4.3BSD systems),
# a newly created file's ctime isn't updated
# until the next sync or stat operation on the file.
ls -l structure/file >/dev/null

# If the time of an initial backup and the creation time of a file contained
# in that backup are the same, the file will be backed up again when an
# incremental backup is done, because the incremental backup backs up
# files created `on or after' the initial backup time.  Without the sleep
# command, behaviour of tar becomes variable, depending whether the system
# clock ticked over to the next second between creating the file and
# backing it up.
sleep 1

tar cf archive --listed=list structure
tar cfv archive --listed=list structure
echo -----
sleep 1
echo y >structure/file
tar cfv archive --listed=list structure

out="\
structure/
-----
structure/
structure/file
"

. $srcdir/after
