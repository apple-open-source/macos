#
#  Makefile.am
#
#  The iODBC driver manager.
#
#  Copyright (C) 1995 by Ke Jin <kejin@empress.com>
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

AUTOMAKE_OPTIONS	= gnu dist-zip 1.4-p5

SUBDIRS 		= admin bin etc man include iodbcinst iodbc iodbcadm drvproxy samples

EXTRA_DIST		= \
	IAFA-PACKAGE \
	LICENSE LICENSE.LGPL LICENSE.BSD \
	README.CVS \
	README.MACOSX \
	bootstrap.sh \
	PORT.OpenLink \
	acinclude.m4 \
	mac/Makefile \
	mac/link-inclibs.sh \
	mac/iodbc-config.macosx \
	mac/README.MYODBC \
	mac/myodbc.configure.diff \
	mac/framework-include.sh \
	mac/iodbc.exp \
	mac/iodbcinst.exp \
	mac/iODBC/English.lproj/InfoPlist.strings \
	mac/iODBC/Info-iODBC.plist \
	mac/iODBC/iODBC.pbproj/project.pbxproj \
	mac/iODBC/iODBC.xcodeproj/project.pbxproj \
	mac/iODBCinst/English.lproj/InfoPlist.strings \
	mac/iODBCinst/Info-iODBCinst.plist \
	mac/iODBCinst/iODBCinst.pbproj/project.pbxproj \
	mac/iODBCinst/iODBCinst.xcodeproj/project.pbxproj \
	mac/iODBCtest/iODBCtest.pbproj/project.pbxproj \
	mac/iODBCtest/iODBCtest.xcodeproj/project.pbxproj \
	mac/iODBCtestw/iODBCtestw.pbproj/project.pbxproj \
	mac/iODBCtestw/iODBCtestw.xcodeproj/project.pbxproj \
	debian/changelog \
	debian/compat \
	debian/control \
	debian/copyright \
	debian/iodbc.files \
	debian/iodbc.install \
	debian/iodbc.lintian-overrides \
	debian/iodbc.undocumented \
	debian/libiodbc2-dev.examples \
	debian/libiodbc2-dev.files \
	debian/libiodbc2-dev.install \
	debian/libiodbc2.docs \
	debian/libiodbc2.examples \
	debian/libiodbc2.files \
	debian/libiodbc2.install \
	debian/libiodbc2.lintian-overrides \
	debian/libiodbc2.undocumented \
	debian/README.Debian \
	debian/rules \
	debian/watch 

# ----------------------------------------------------------------------
#
#  Maintainers only
# 
# ----------------------------------------------------------------------

MAINTAINERCLEANFILES	= Makefile.in aclocal.m4 configure
DISTCLEANFILES		= config.nice


if MAINTAINER_MODE

#
#  Create Linux RPM's
#
RPMFLAGS=--define="_topdir `pwd`/distrib"

linux-rpm:
	$(mkinstalldirs) distrib/SOURCES distrib/SRPMS distrib/SPECS
	$(mkinstalldirs) distrib/BUILD distrib/RPMS/i386
	$(MAKE) dist
	cp $(PACKAGE)-$(VERSION).tar.gz distrib/SOURCES
	rpmbuild $(RPMFLAGS) -ba admin/libiodbc.spec
	rpmbuild $(RPMFLAGS) --clean --rmsource admin/libiodbc.spec

#
#  Create a tar file containing the library and include files
#
binary-tar:
	-mkdir @build@
	$(MAKE) install prefix=`pwd`/@build@
	tar cvf @build@.tar @build@
	gzip -9vf @build@.tar
	rm -rf @build@

#
#  Create a source snapshot package
#
snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`


#
#  Create an official release package
#
release:
	$(MAKE) distcheck

#
#  Generate ChangeLog
#
changelog:
	cvs2cl --no-wrap --utc --hide-filenames --prune --window 3600

endif
