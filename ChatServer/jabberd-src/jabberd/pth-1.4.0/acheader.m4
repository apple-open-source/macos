dnl ##
dnl ##  GNU Pth - The GNU Portable Threads
dnl ##  Copyright (c) 1999-2001 Ralf S. Engelschall <rse@engelschall.com>
dnl ##
dnl ##  This file is part of GNU Pth, a non-preemptive thread scheduling
dnl ##  library which can be found at http://www.gnu.org/software/pth/.
dnl ##
dnl ##  This library is free software; you can redistribute it and/or
dnl ##  modify it under the terms of the GNU Lesser General Public
dnl ##  License as published by the Free Software Foundation; either
dnl ##  version 2.1 of the License, or (at your option) any later version.
dnl ##
dnl ##  This library is distributed in the hope that it will be useful,
dnl ##  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl ##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
dnl ##  Lesser General Public License for more details.
dnl ##
dnl ##  You should have received a copy of the GNU Lesser General Public
dnl ##  License along with this library; if not, write to the Free Software
dnl ##  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
dnl ##  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
dnl ##
dnl ##  acheader.m4: Pth Autoheader macros
dnl ##

dnl #   These macros are replacement macros for use with `autoheader' only !!

define([AC_CHECK_FUNCTION], [#
@@@funcs="$funcs $1"@@@
ifelse([$2], , , [
# If it was found, we do:
$2
# If it was not found, we do:
$3
])
])

define([AC_CHECK_FUNCTIONS], [#
@@@funcs="$funcs $1"@@@
ifelse([$2], , , [
# If it was found, we do:
$2
# If it was not found, we do:
$3
])
])

