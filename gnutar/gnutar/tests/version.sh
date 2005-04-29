#! /bin/sh

# Check if the proper version is being tested.

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

if test -n "`$PACKAGE --version | sed -n s/$PACKAGE.*$VERSION/OK/p`"; then
  banner="Regression testing for GNU $PACKAGE, version $VERSION"
  dashes=`echo $banner | sed s/./=/g`
  echo $dashes
  echo $banner
  echo $dashes
else
  echo '=============================================================='
  echo 'WARNING: Not using the proper version, *all* checks dubious...'
  echo '=============================================================='
  exit 1
fi
