#
#  Makefile.am
#
#  $Id: Makefile.am,v 1.1.1.2 2002/04/30 00:40:21 miner Exp $
#
#  The iODBC driver manager.
#  
#  Copyright (C) 1999-2002 by OpenLink Software <iodbc@openlinksw.com>
#  All Rights Reserved.
#
#  This software is released under the terms of either of the following
#  licenses:
#
#      - GNU Library General Public License (see LICENSE.LGPL) 
#      - The BSD License (see LICENSE.BSD).
#
#  While not mandated by the BSD license, any patches you make to the
#  iODBC source code may be contributed back into the iODBC project
#  at your discretion. Contributions will benefit the Open Source and
#  Data Access community as a whole. Submissions may be made at:
#
#      http://www.iodbc.org
#
#
#  GNU Library Generic Public License Version 2
#  ============================================
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Library General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
#
#  You should have received a copy of the GNU Library General Public
#  License along with this library; if not, write to the Free
#  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#
#  The BSD License
#  ===============
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#  3. Neither the name of OpenLink Software Inc. nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL OPENLINK OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if GUI
SUBDIRS			= gtk

lib_LTLIBRARIES		= libdrvproxy.la
endif

if GTK
INCLUDES		= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/iodbc \
	-I$(top_srcdir)/iodbcinst \
	@GTK_CFLAGS@ -D__GTK__
else
INCLUDES		= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/iodbc \
	-I$(top_srcdir)/iodbcinst
endif

libdrvproxy_la_LDFLAGS	= -version-info @lib_version@ \
			  -export-symbols $(srcdir)/drvproxy.exp

if GTK
libdrvproxy_la_LIBADD	= \
	$(LIBADD_DL) \
	../iodbc/dlf.lo \
	../drvproxy/gtk/confirm.lo \
	../drvproxy/gtk/gensetup.lo \
	-L../iodbcinst -liodbcinst \
	-L../iodbcadm -liodbcadm \
	@GTK_LIBS@
else
libdrvproxy_la_LIBADD	= \
	$(LIBADD_DL) \
	../iodbc/dlf.lo \
	-L../iodbcinst -liodbcinst \
	-L../iodbcadm -liodbcadm
endif

libdrvproxy_la_SOURCES	= \
	ConfigDSN.c \
	ConfigDriver.c \
	Info.c \
	drvconn.c \
	login.c

noinst_HEADERS		= gui.h

EXTRA_DIST		= drvproxy.exp
