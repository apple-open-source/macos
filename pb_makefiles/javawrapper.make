#
# javawrapper.make
#
# Variable definitions and rules for building wrapper libraries.  A wrapper
# library is a library that exposes an Objective C library or framework in
# Java.
#

include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

.PHONE: library all
library: all
PROJTYPE = LIBRARY
override DEBUG_SUFFIX = _g

ifeq "NEXTSTEP" "$(OS)"
DEFAULT_OBJC_LIBRARY_DIR = $(NEXT_ROOT)/usr/lib/java
else
ifeq "MACOS" "$(OS)"
DEFAULT_OBJC_LIBRARY_DIR = $(NEXT_ROOT)/usr/lib/java
else
ifeq "WINDOWS" "$(OS)"
DEFAULT_OBJC_LIBRARY_DIR = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries
else
DEFAULT_OBJC_LIBRARY_DIR = $(NEXT_ROOT)$(SYSTEM_LIBRARY_EXECUTABLES_DIR)
endif
endif
endif

ifeq "" "$(OBJC_JAVA_LIBRARY)"
ifeq "YES-WINDOWS" "$(DEBUG)-$(OS)"
OBJC_JAVA_LIBRARY = -L$(DEFAULT_OBJC_LIBRARY_DIR) -lObjCJava_g
else
OBJC_JAVA_LIBRARY = -L$(DEFAULT_OBJC_LIBRARY_DIR) -lObjCJava
endif
endif


ifneq "" "$(JOBS_FILES)"

VM_INCLUDE_CFLAGS := $(addprefix -I,$(shell javaconfig Headers))

# Radar #2371453 Include FRAMEWORK_PATHS and HEADER_PATHS in the Bridget invocation.
BRIDGET_CFLAGS = -I. -I$(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/Java/Headers -I$(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Java/Headers $(VM_INCLUDE_CFLAGS) $(FRAMEWORK_PATHS) $(HEADER_PATHS)

#
# RDW 09/14/1999 -- Added behavior that will cause an appropriately
#                   named symbolic link to the wrapper library
#                   in its usual place and name to make life easier
#                   for developers building wrappers as shared
#                   libraries.  This addition is for Solaris & 
#                   HP-UX. 
#
#                   Need to define WRAPPER_SHLIB_INSTALLDIR and
#                   PUBLIC_SHLIB_LINK.  Also, need to set
#                   RELATIVE_WRAPPER_SHLIB_PATH in preparation
#                   for the symlink performed in "install-wrapper-
#                   shlib" below.  The assumption here is that
#                   WRAPPER_SHLIB_INSTALLDIR will be under NEXT_ROOT.
#
#     09/17/1999 -- Added logic that will cause an additional
#                   target to be used to support builds better.
#

ifeq "$(OS)" "SOLARIS"
PDO_UNIX_WRAPPER = YES
endif 
ifeq "$(OS)" "HPUX" 
PDO_UNIX_WRAPPER = YES
endif

ifeq "$(PDO_UNIX_WRAPPER)" "YES"
ifneq "$(LIBRARY_STYLE)" "STATIC"

AFTER_INSTALL += install-wrapper-shlib
AFTER_BUILD   += create-shlib-links-for-wrapper-build
PUBLIC_SHLIB_LINK = $(LIBRARY_PREFIX)$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT)
RELATIVE_WRAPPER_SHLIB_PATH = ../Frameworks/ObjCJava.framework/Resources/
WRAPPER_SHLIB_INSTALLDIR = $(SYSTEM_LIBRARY_EXECUTABLES_DIR)

endif
endif

ifeq "$(PLATFORM_OS)" "solaris"

BRIDGET_CFLAGS += -I$(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries/gcc-lib/sparc-nextpdo-solaris2/include -I$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/System.framework/Headers -F$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks
BRIDGET_LIBFLAGS = $(BRIDGET_LIBNAMEFLAGS)

else

ifeq "$(PLATFORM_OS)" "hpux"

#
# RDW 09/10/1999 -- Added "-D__hpux -D__STDC_EXT__" so that Java wrapper
#                   projects, which build against the JDK, are able to
#                   find the right set of definitions and declarations
#                   for types, etc.
#

BRIDGET_CFLAGS += -I$(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries/gcc-lib/hppa1.1-nextpdo-hpux/2.7.2.1/include -I$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/System.framework/Headers -F$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks -D__hpux -D__STDC_EXT__
BRIDGET_LIBFLAGS = $(BRIDGET_LIBNAMEFLAGS)

else

BRIDGET_LIBFLAGS = $(BRIDGET_LIBNAMEFLAGS) $(BRIDGET_LIBVERSIONFLAGS)

endif
endif

BRIDGET_FLAGS = $(BRIDGET_LIBFLAGS)

ALL_BRIDGET_CFLAGS = $(BRIDGET_CFLAGS) $(OTHER_BRIDGET_CFLAGS)
ALL_BRIDGET_FLAGS = $(ALL_BRIDGET_CFLAGS) $(BRIDGET_FLAGS) $(OTHER_BRIDGET_FLAGS)

OTHER_CFLAGS += $(ALL_BRIDGET_CFLAGS)

OTHER_LIBS += $(OBJC_JAVA_LIBRARY)

#
# Need to work around a crasher in cpp-precomp by using the traditional cpp
#
ifeq "NEXTSTEP" "$(OS)"
OTHER_CFLAGS += -traditional-cpp
else
ifeq "MACOS" "$(OS)"
OTHER_CFLAGS += -no-cpp-precomp
endif
endif

override NAME := $(shell $(BRIDGET) $(ALL_BRIDGET_FLAGS) -listlibname $(JOBS_FILES))

BRIDGET_GENERATED_SRCFILES := $(shell $(BRIDGET) $(ALL_BRIDGET_FLAGS) -listobjc $(JOBS_FILES))
OTHER_GENERATED_SRCFILES += $(BRIDGET_GENERATED_SRCFILES)

BRIDGET_GENERATED_OFILES = $(BRIDGET_GENERATED_SRCFILES:.m=.o)
OFILELIST_PRODUCT = $(OFILE_DIR)/bridget_generated.ofileList
OTHER_OFILELISTS += $(OFILELIST_PRODUCT)

BRIDGET_GENERATED_JAVA_CLASSES := $(shell $(BRIDGET) $(ALL_BRIDGET_FLAGS) -listjava $(JOBS_FILES))
OTHER_GENERATED_JAVA_CLASSES += $(addprefix $(SFILE_DIR)/,$(BRIDGET_GENERATED_JAVA_CLASSES))

BEFORE_PREBUILD += generate_stub_code

ifneq "NO" "$(INSTALL_JOBS_FILE)"
BEFORE_INSTALL += install-jobs-file
endif

ifneq "NEXTSTEP" "$(OS)"
ifneq "MACOS" "$(OS)"
ifneq "YES" "$(DEBUG)"
ifneq "YES" "$(SUPPRESS_JAVA_DEBUG_INSTALLATION)"
AFTER_INSTALL += install_java_debug
endif
endif
endif
endif

ifneq "" "$(wildcard $(NAME).h)"
OBJCJAVA_HEADERS = $(NAME).h
endif

OBJCJAVA_HEADERS += $(OTHER_OBJCJAVA_HEADERS)

ifneq "NO" "$(INSTALL_OBJCJAVA_HEADERS)"
ifneq "" "$(OBJCJAVA_HEADERS)"
BEFORE_INSTALL += install-objcjava-headers
endif
endif

endif

ARCHIVE_JAVA_CLASSES = NO

#
# RDW 09/17/1999 -- Use versioned names for the product on HP-UX
#                   and Solaris. If versioning is enabled, DYLIB_INSTALL_NAME is changed in versions.make.
#
ifeq "$(PDO_UNIX_WRAPPER)" "YES"
PRODUCT = $(PRODUCT_DIR)/$(DYLIB_INSTALL_NAME)
else
PRODUCT = $(PRODUCT_DIR)/$(LIBRARY_PREFIX)$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT)
endif
PRODUCTS = $(PRODUCT)
STRIPPED_PRODUCTS = $(PRODUCT)
JAVA_RESOURCE_DIR = $(PRODUCT_DIR)
JAVA_RESOURCE_DIR_CLIENT = $(PRODUCT_DIR)

LINK_SUBPROJECTS = NO
RECURSIVE_FLAG += "LINK_SUBPROJECTS = NO"

ifndef LIBRARY_LIB_INSTALLDIR
LIBRARY_LIB_INSTALLDIR = $(SYSTEM_DEVELOPER_DIR)/Libraries
endif

DYLIB_INSTALL_DIR = $(DYLIB_ROOT)$(INSTALLDIR)
DYLIB_INSTALL_NAME = $(LIBRARY_PREFIX)$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT)
INSTALL_NAME_DIRECTIVE = -install_name $(DYLIB_INSTALL_DIR)/$(DYLIB_INSTALL_NAME)

ifneq "STATIC" "$(LIBRARY_STYLE)"
PROJTYPE_LDFLAGS = -dynamic -compatibility_version $(COMPATIBILITY_PROJECT_VERSION) -current_version $(CURRENT_PROJECT_VERSION) $(INSTALL_NAME_DIRECTIVE)
else
PROJTYPE_LDFLAGS = -static
endif

BEFORE_INSTALL += verify-install-name-directive
BEFORE_INSTALL += copy-java-classes

ifeq "WINDOWS" "$(OS)"
ifneq "STATIC" "$(LIBRARY_STYLE)"

PRODUCT_CRUFT = $(PRODUCT_DIR)/$(LIBRARY_PREFIX)$(NAME)$(BUILD_TYPE_SUFFIX)$(EXP_EXT)
BEFORE_INSTALL += install-lib
OS_LDFLAGS += -def $(WINDOWS_DEF_FILE)

endif
endif

include $(MAKEFILEDIR)/common.make

ifneq "" "$(ENABLE_VERSIONING)"
BRIDGET_LIBVERSIONFLAGS = -libversion "$(VERSION_NAME)"
endif

SRCFILES += $(JOBS_FILES) $(HIDDEN_JOBS_FILES)

ifneq "NO" "$(INSTALL_JOBS_FILE)"
ifeq "" "$(JOBS_INSTALL_DIR)"
JOBS_INSTALL_DIR = $(SYSTEM_DEVELOPER_DIR)/Java/Jobs
endif
endif

ifneq "NO" "$(INSTALL_OBJCJAVA_HEADERS)"
ifeq "" "$(OBJCJAVA_HEADERS_INSTALL_DIR)"
OBJCJAVA_HEADERS_INSTALL_DIR = $(SYSTEM_DEVELOPER_DIR)/Java/Headers
endif
endif

ifeq "" "$(JAVA_INSTALL_DIR)"
JAVA_INSTALL_DIR = $(SYSTEM_LIBRARY_DIR)/Java
endif

-include $(LOCAL_MAKEFILEDIR)/javawrapper.make.preamble

ifeq "STATIC" "$(LIBRARY_STYLE)"

$(PRODUCT): $(DEPENDENCIES)
ifeq "$(USE_AR)" "YES"
	$(AR) ru $(PRODUCT) $(LOADABLES)
	$(RANLIB) $(PRODUCT)
else
	$(LIBTOOL) $(ALL_LIBTOOL_FLAGS) -o $(PRODUCT) $(LOADABLES)
endif

else

$(PRODUCT): $(DEPENDENCIES) $(WINDOWS_DEF_FILE)
	$(LIBTOOL) $(filter-out -g, $(ALL_LDFLAGS)) -o $(PRODUCT) $(LOADABLES)
ifneq "$(PRODUCT_CRUFT)" ""
	$(RM) -f $(PRODUCT_CRUFT)
endif
endif

verify-install-name-directive:
ifeq "" "$(INSTALL_NAME_DIRECTIVE)"
	$(SILENT) $(ECHO) You must restore the INSTALL_NAME_DIRECTIVE variable
	$(SILENT) $(ECHO) before installing a framework.
	$(SILENT) exit 1
endif

install-lib: $(DSTROOT)$(LIBRARY_LIB_INSTALLDIR)
	$(RM) -f $(DSTROOT)$(INSTALLDIR)/$(NAME)$(BUILD_TYPE_SUFFIX).lib
	$(CP) $(PRODUCT_DIR)/$(NAME)$(BUILD_TYPE_SUFFIX).lib $(DSTROOT)$(LIBRARY_LIB_INSTALLDIR)

#
# RDW 09/14/1999 -- Added behavior that will cause an appropriately
#                   named symbolic link to the wrapper library 
#                   in its usual place and name to make life easier
#                   for developers building frameworks as shared
#                   libraries.  This addition is for Solaris &
#                   HP-UX.
#
#                   This behavior is represented by
#                   "install-wrapper-shlib".
#
#     09/17/1999 -- Added versioning support.  Also, added an
#                   an additional target "create-shlib-links-for-
#                   wrapper-build" to support builds better.
#
install-wrapper-shlib: $(DSTROOT)$(WRAPPER_SHLIB_INSTALLDIR)
	[ -h "$(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(DYLIB_INSTALL_NAME)" ] && $(RM) -f $(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(DYLIB_INSTALL_NAME) || :
	[ ! -f "$(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(DYLIB_INSTALL_NAME)" ] && $(SYMLINK) $(RELATIVE_WRAPPER_SHLIB_PATH)/$(DYLIB_INSTALL_NAME) $(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(DYLIB_INSTALL_NAME) || :
	[ -h "$(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(PUBLIC_SHLIB_LINK)" ] && $(RM) -f $(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(PUBLIC_SHLIB_LINK) || :
	[ ! -f "$(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(PUBLIC_SHLIB_LINK)" ] && $(SYMLINK) $(DYLIB_INSTALL_NAME) $(DSTROOT)/$(WRAPPER_SHLIB_INSTALLDIR)/$(PUBLIC_SHLIB_LINK) || :

create-shlib-links-for-wrapper-build:
	[ -h "$(PRODUCT_DIR)/$(PUBLIC_SHLIB_LINK)" ] && $(RM) -f $(PRODUCT_DIR)/$(PUBLIC_SHLIB_LINK) || :
	[ ! -f "$(PRODUCT_DIR)/$(PUBLIC_SHLIB_LINK)" ] && $(SYMLINK) $(DYLIB_INSTALL_NAME) $(PRODUCT_DIR)/$(PUBLIC_SHLIB_LINK) || :

#
# creating directories
#

$(DSTROOT)$(LIBRARY_LIB_INSTALLDIR) $(DSTROOT)$(JAVA_INSTALL_DIR) $(DSTROOT)$(JOBS_INSTALL_DIR) $(DSTROOT)$(OBJCJAVA_HEADERS_INSTALL_DIR) $(DSTROOT)$(WRAPPER_SHLIB_INSTALLDIR):
	$(MKDIRS) $@

generate_stub_code: $(SFILE_DIR) $(SFILE_DIR)/.bridget

$(SFILE_DIR)/.bridget: $(JOBS_FILES) $(OTHER_JOBS_FILES)
	$(BRIDGET) -o $(SFILE_DIR) $(ALL_BRIDGET_FLAGS) $(JOBS_FILES)
	touch $(SFILE_DIR)/.bridget

$(OFILELIST_PRODUCT): $(BRIDGET_GENERATED_OFILES) Makefile
	$(OFILE_LIST_TOOL) $(BRIDGET_GENERATED_OFILES) -o $(OFILELIST_PRODUCT)

install-jobs-file: $(DSTROOT)$(JOBS_INSTALL_DIR)
	$(SILENT) $(FASTCP) $(JOBS_FILES) $(OTHER_JOBS_FILES) $(DSTROOT)$(JOBS_INSTALL_DIR)

install-objcjava-headers: $(DSTROOT)$(OBJCJAVA_HEADERS_INSTALL_DIR)
	$(SILENT) $(FASTCP) $(OBJCJAVA_HEADERS) $(DSTROOT)$(OBJCJAVA_HEADERS_INSTALL_DIR)

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

-include $(LOCAL_MAKEFILEDIR)/javawrapper.make.postamble

