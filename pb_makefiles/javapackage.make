##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# javapackage.make
#
# Variable definitions and rules for building java packages.  A package is
# a zip file containing a number of java .class files and any resources
# needed by those files.
#
# PUBLIC TARGETS
#    javapackage: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

# Standard goop.
.PHONY: package all
package: all
PROJTYPE = JAVAPACKAGE
override DEBUG_SUFFIX = _g

# this OFILES definition is copied directly from common.make
# we need the list of ofiles *before* we include common.make, because
# we need to know whether to include directly, or indirectly through
# library.make
OFILES = $(CLASSES:.m=.o) $(MFILES:.m=.o) $(CFILES:.c=.o) $(CAPCFILES:.C=.o) $(CAPMFILES:.M=.o) $(CCFILES:.cc=.o) $(CPPFILES:.cpp=.o) $(CXXFILES:.cxx=.o) $(PSWFILES:.psw=.o) $(PSWMFILES:.pswm=.o) $(PROJTYPE_OFILES) $(OTHERLINKEDOFILES) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES)

# javapackages often need to use the JDK header files and library
# These are defined here, but only automatically added if the package
# generates .o files
VM_INCLUDE_CFLAGS := $(addprefix -I,$(shell javaconfig Headers))
ifeq "YES-WINDOWS" "$(DEBUG)-$(OS)"
JAVA_VM_LIB := $(shell javaconfig DebugLibrary)
else
JAVA_VM_LIB := $(shell javaconfig Library)
endif

# if a javapackage contains any non-java source files it is actually a library
JAVA_RESOURCE_DIR = $(PRODUCT_DIR)
JAVA_RESOURCE_DIR_CLIENT = $(PRODUCT_DIR)
ifeq "" "$(strip $(OFILES))"
PRODUCT = 
PRODUCTS =
JAVA_ENABLED = YES
STRIP_ON_INSTALL = NO
include $(MAKEFILEDIR)/common.make
else
OTHER_CFLAGS += $(VM_INCLUDE_CFLAGS)
OTHER_LIBS += $(JAVA_VM_LIB)
ifneq "MACOS" "$(OS)"
ifneq "NEXTSTEP" "$(OS)"
ifneq "YES" "$(DEBUG)"
ifneq "YES" "$(SUPPRESS_JAVA_DEBUG_INSTALLATION)"
AFTER_INSTALL += install_java_debug
endif
endif
endif
endif
include $(MAKEFILEDIR)/library.make
endif

-include $(LOCAL_MAKEFILEDIR)/javapackage.make.preamble

JAVA_INSTALL_DIR = $(INSTALLDIR)

copy-java-resources: copy-java-classes

ifeq "$(JAVA_PRODUCT)" ""
install-projtype-specific-products:
	$(MKDIRS) $(DSTROOT)$(JAVA_INSTALL_DIR)
	-cd $(JAVA_OBJ_DIR) && files=`find . -print` && $(CD) $(DSTROOT)$(JAVA_INSTALL_DIR) && $(CHMOD) +w $$files > $(NULL) 2>&1
	-cd $(JAVA_OBJ_DIR) && files=`find . -type f -print` && $(CD) $(DSTROOT)$(JAVA_INSTALL_DIR) && $(RM) -f $$files
	($(CD) $(JAVA_OBJ_DIR) && $(TAR) cf - . ) | ($(CD) $(DSTROOT)$(JAVA_INSTALL_DIR) && $(TAR) xf -)
else
install-projtype-specific-products: $(DSTROOT)$(INSTALLDIR)
	-$(CHMOD) -R +w $(INSTALLED_JAVA_PRODUCT)
	$(RM) -rf $(INSTALLED_JAVA_PRODUCT)
	($(CD) $(PRODUCT_DIR) && $(TAR) cf - $(notdir $(JAVA_PRODUCT))) | ($(CD) $(DSTROOT)$(INSTALLDIR) && $(TAR) xf -)
endif

-include $(LOCAL_MAKEFILEDIR)/javapackage.make.postamble
