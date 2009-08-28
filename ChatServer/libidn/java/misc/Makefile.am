## Process this file with automake to produce Makefile.in
# Copyright (C) 2004, 2005, 2006, 2007  Free Software Foundation
#
# This file is part of GNU Libidn.
#
# GNU Libidn is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# GNU Libidn is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GNU Libidn; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

JAVAROOT = $(top_builddir)/java/misc
AM_JAVACFLAGS = -classpath $(top_builddir)/java/libidn-@VERSION@.jar:$(top_srcdir)/java/libidn-@VERSION@.jar

if JAVA
dist_noinst_JAVA = GenerateRFC3454.java GenerateNFKC.java	\
			TestIDNA.java TestNFKC.java
endif # JAVA
