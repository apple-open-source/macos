#
#  Makefile.mac
#
#  The iODBC driver manager.
#  
#  Copyright (C) 1996-2002 by OpenLink Software <iodbc@openlinksw.com>
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


all: iODBCinst iODBC iODBCtest iODBCtestw

#
# ascertain whether to use pbxbuild (jaguar) or xcodebuild (panther)
#

PROJBUILD:=$(shell (ls -1 /usr/bin/{pbxbuild,xcodebuild}|head -1)2>/dev/null)

#
#  Build the libraries 
#
.PHONY: iODBCinst iODBC
iODBC: include iODBCinst
	cd iODBC; $(PROJBUILD) build -buildstyle Deployment

iODBCinst: include 
	echo "Projbuild is [$(PROJBUILD)]"
	cd iODBCinst; $(PROJBUILD) build -buildstyle Deployment

#
#  Build the test program
#
.PHONY: iODBCtest iODBCtestw
iODBCtest:
	cd iODBCtest; $(PROJBUILD) build -buildstyle Deployment

iODBCtestw:
	cd iODBCtestw ; $(PROJBUILD) build -buildstyle Deployment

#
#  Generate iODBC framework style include files
#
.PHONY: include
include: framework-include.sh ../include/sql.h ../include/sqlext.h ../include/sqltypes.h ../include/iodbcext.h ../include/iodbcunix.h ../include/iodbcinst.h
	sh framework-include.sh

#
#  Install libraries in /Library/Frameworks
#
install:
	-sudo rm /Library/Frameworks/iODBC.framework/Versions/Current
	-sudo rm /Library/Frameworks/iODBCinst.framework/Versions/Current
	cd iODBC;     sudo $(PROJBUILD) install -buildstyle Deployment DSTROOT=/
	cd iODBCinst; sudo $(PROJBUILD) install -buildstyle Deployment DSTROOT=/
	sudo sh link-inclibs.sh

#
#  Clean out the targets
#
clean:
	rm -rf include
	cd iODBC;     $(PROJBUILD) clean
	cd iODBCinst; $(PROJBUILD) clean
	cd iODBCtest; $(PROJBUILD) clean
	cd iODBCtestw; $(PROJBUILD) clean
