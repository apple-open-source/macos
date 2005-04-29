#! /bin/sh

# Unreadable directories yielded error despite --ignore-failed-read.

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

> check-uid
set - x`ls -l check-uid`
uid_name="$3"
set - x`ls -ln check-uid`
uid_number="$3"
if test "$uid_name" = root || test "$uid_number" = 0; then

  # The test is meaningless for super-user.
  rm check-uid

else

   touch file
   mkdir directory
   touch directory/file

   echo 1>&2 -----
   chmod 000 file
   tar cf archive file
   status=$?
   chmod 600 file
   test $status = 2 || exit 1

   echo 1>&2 -----
   chmod 000 file
   tar cf archive --ignore-failed-read file || exit 1
   status=$?
   chmod 600 file
   test $status = 0 || exit 1

   echo 1>&2 -----
   chmod 000 directory
   tar cf archive directory
   status=$?
   chmod 700 directory
   test $status = 2 || exit 1

   echo 1>&2 -----
   chmod 000 directory
   tar cf archive --ignore-failed-read directory || exit 1
   status=$?
   chmod 700 directory
   test $status = 0 || exit 1

   err="\
-----
tar: file: Cannot open: Permission denied
tar: Error exit delayed from previous errors
-----
tar: file: Warning: Cannot open: Permission denied
-----
tar: directory: Cannot savedir: Permission denied
tar: Error exit delayed from previous errors
-----
tar: directory: Warning: Cannot savedir: Permission denied
"

fi

. $srcdir/after
