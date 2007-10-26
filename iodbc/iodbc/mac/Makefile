#
#  Makefile.mac
#
#  The iODBC driver manager.
#
#  Copyright (C) 1996-2006 by OpenLink Software <iodbc@openlinksw.com>
#  All Rights Reserved.
#
#  This software is released under the terms of either of the following
#  licenses:
#
#      - GNU Library General Public License (see LICENSE.LGPL)
#      - The BSD License (see LICENSE.BSD).
#
#  Note that the only valid version of the LGPL license as far as this
#  project is concerned is the original GNU Library General Public License
#  Version 2, dated June 1991.
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
#  License as published by the Free Software Foundation; only
#  Version 2 of the License dated June 1991.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
#
#  You should have received a copy of the GNU Library General Public
#  License along with this library; if not, write to the Free
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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
#


#
#  Determine build model (Deployment/Development)
#
ifeq (,$(MODEL))
MODEL	:= Deployment
endif


#
#  Determine which version of Mac OS X we are running
#
macosx_version	:= $(shell sw_vers -productVersion)

#
#  Mac OS X 10.2
#
ifeq (10.2, $(findstring 10.2, $(macosx_version)))
    PROJBUILD := /usr/bin/pbxbuild -buildstyle $(MODEL)
endif

#
#  Mac OS X 10.3
#
ifeq (10.3, $(findstring 10.3, $(macosx_version)))
    PROJBUILD := /usr/bin/xcodebuild -buildstyle $(MODEL)
endif

#
#  Mac OS X 10.4
#
ifeq (10.4, $(findstring 10.4, $(macosx_version)))
    PROJBUILD := /usr/bin/xcodebuild -configuration $(MODEL)
endif


#
#  Build the libraries
#
all: include
	@echo "Building on Mac OS X $(macosx_version)"
	@echo "Projbuild is [$(PROJBUILD)]"
	cd iODBCinst;	$(PROJBUILD) build
	cd iODBC;	$(PROJBUILD) build
	cd iODBCtest;	$(PROJBUILD) build
	cd iODBCtestw;	$(PROJBUILD) build


#
#  Generate iODBC framework style include files
#
include: framework-include.sh ../include/sql.h ../include/sqlext.h ../include/sqltypes.h ../include/iodbcext.h ../include/iodbcunix.h ../include/odbcinst.h
	sh framework-include.sh
	touch include


#
#  Install libraries in /Library/Frameworks
#
install:
	-sudo rm /Library/Frameworks/iODBC.framework/Versions/Current
	-sudo rm /Library/Frameworks/iODBCinst.framework/Versions/Current
	cd iODBCinst;	sudo $(PROJBUILD) install DSTROOT=/
	cd iODBC;	sudo $(PROJBUILD) install DSTROOT=/
	cd iODBCtest;	sudo $(PROJBUILD) install DSTROOT=/
	cd iODBCtestw;	sudo $(PROJBUILD) install DSTROOT=/
	sudo sh link-inclibs.sh


#
#  Clean out the targets
#
clean:
	rm -rf include
	cd iODBC;	$(PROJBUILD) clean
	cd iODBCinst;	$(PROJBUILD) clean
	cd iODBCtest;	$(PROJBUILD) clean
	cd iODBCtestw;	$(PROJBUILD) clean

realclean: clean
	rm -rf iODBC/build iODBCinst/build iODBCtest/build iODBCtestw/build
