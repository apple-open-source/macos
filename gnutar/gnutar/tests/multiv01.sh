#! /bin/sh

# Test multivolume dumps from pipes.

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

# Fixme: should be configurable
#  TRUSS=truss -o /tmp/tr
#  TRUSS=strace
set -e

genfile --length 7168 > file1

for block in " 1" " 2" " 3" " 4" " 5" " 6" " 7" " 8" \
              " 9" "10" "11" "12" "13" "14" "15" "16" ; do \
  echo "file2  block ${block} bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla"
  for count in 2 3 4 5 6 7 8 ; do
    echo "bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla"
  done
done >file2

tar -c --multi-volume --tape-length=10 \
  --listed-incremental=t.snar \
  -f t1-pipe.tar -f t2-pipe.tar ./file1 ./file2

mkdir extract-dir-pipe
dd bs=4096 count=10 if=t2-pipe.tar 2>/dev/null |
PATH=$PATH ${TRUSS} tar -f t1-pipe.tar -f - \
      -C extract-dir-pipe -x --multi-volume \
      --tape-length=10 --read-full-records

cmp file1 extract-dir-pipe/file1
cmp file2 extract-dir-pipe/file2

out="\
"

. $srcdir/after
