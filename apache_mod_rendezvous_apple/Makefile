#Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
#
#License for apache_mod_rendezvous_apple module:
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice, this list of 
# conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice, this list 
# of conditions and the following disclaimer in the documentation and/or other materials 
# provided with the distribution.
# 3. The end-user documentation included with the redistribution, if any, must include 
# the following acknowledgment:
#         "This product includes software developed by Apple Computer, Inc."
# Alternately, this acknowledgment may appear in the software itself, if and 
# wherever such third-party acknowledgments normally appear.
# 4. The names "Apache", "Apache Software Foundation", "Apple" and "Apple Computer, Inc." 
# must not be used to endorse or promote products derived from this software without 
# prior written permission. For written permission regarding the "Apache" and 
# "Apache Software Foundation" names, please contact apache@apache.org.
# 5. Products derived from this software may not be called "Apache" or "Apple", 
# nor may "Apache" or "Apple" appear in their name, without prior written 
# permission of the Apache Software Foundation, or Apple Computer, Inc., respectively.
# 
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, 
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE COMPUTER, INC., 
# THE APACHE SOFTWARE FOUNDATION OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
# OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Except as expressly set forth above, nothing in this License shall be construed 
# as granting licensee, expressly or by implication, estoppel or otherwise, any 
# rights or license under any trade secrets, know-how, patents, registrations, 
# copyrights or other intellectual property rights of Apple, including without 
# limitation, application programming interfaces ("APIs") referenced by the code, 
# the functionality implemented by the APIs and the functionality invoked by calling 
# the APIs.
# 

MODULE_NAME = mod_rendezvous_apple
MODULE_SRC = $(MODULE_NAME).c
MODULE = $(MODULE_NAME).so
OTHER_SRC = 
HEADERS = 
SRCFILES = Makefile $(MODULE_SRC) $(OTHER_SRC) $(HEADERS)
NEXTSTEP_INSTALLDIR := $(shell /usr/sbin/apxs -q LIBEXECDIR)

MORE_FLAGS = -Wc,"$(RC_CFLAGS)"
MORE_FLAGS += -Wl,"-bundle_loader /usr/sbin/httpd $(RC_CFLAGS) -framework DirectoryService"

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

all build $(MODULE): $(MODULE_SRC) $(OTHER_SRC)
	/usr/sbin/apxs -c  $(MORE_FLAGS) -o $(MODULE) $(MODULE_SRC) $(OTHER_SRC)
 
installsrc:
	@echo "Installing source files..."
	-$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) cf - $(SRCFILES) | (cd $(SRCROOT)$(SRCPATH) && $(TAR) xf -)

installhdrs:
	@echo "Installing header files..."

install: $(MODULE)
	@echo "Installing product..."
	$(MKDIRS) $(SYMROOT)$(NEXTSTEP_INSTALLDIR)
	$(CP) $(MODULE) $(SYMROOT)$(NEXTSTEP_INSTALLDIR)
	$(CHMOD) 755 $(SYMROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE)
	$(MKDIRS) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)
	$(STRIP) -x $(SYMROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE) -o $(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE)

clean:
	@echo "== Cleaning $(MODULE_NAME) =="
	-$(RM) $(MODULE) *.o
