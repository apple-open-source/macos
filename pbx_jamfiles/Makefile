#
#  Custom makefile to generate the ProjectBuilder-Jambase. This is not a
#  ProjectBuilder project due to bootstrapping issues.
#
#  Copyright (c) 1999,2000 Apple Computer, Inc.
#  All rights reserved.
#

OS=MACOS
MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make



# Name of the project
PROJECT_NAME = pbx_jamfiles

# Name of the aggregate product to produce
JAMBASE_NAME = ProjectBuilderJambase

# Where to install it
INSTALL_PATH = $(SYSTEM_DEVELOPER_DIR)/Makefiles/pbx_jamfiles

# The following are concatenated together to produce $(JAMBASE_NAME)
JAMBASE_SOURCES = Jambase util.jam compatibility.jam commands.jam files.jam products.jam process.jam actions.jam include-jamfile.jam

# The following are installed into the same directory as $(JAMBASE_NAME)
JAMBASE_RESOURCES = platform-darwin.jam platform-macos.jam

# Other source code files that also need to be installsrc:ed
OTHER_SOURCES = Makefile CVSVersionInfo.txt

# Set up some sensible defaults
SRCROOT ?= .
SYMROOT ?= .
DSTROOT ?= /tmp/$(PROJECT_NAME).dst
ifeq "" "$(INSTALL_AS_USER)"
    INSTALL_AS_USER = $(USER)
endif
ifeq "" "$(INSTALL_AS_GROUP)"
    INSTALL_AS_GROUP = $(GROUP)
endif
ifeq "" "$(INSTALL_PERMISSIONS)"
    INSTALL_PERMISSIONS = o+rX
endif




all: $(JAMBASE_NAME)

$(JAMBASE_NAME): $(JAMBASE_SOURCES)
	$(CAT) $(JAMBASE_SOURCES) > $(SYMROOT)/$(JAMBASE_NAME)

install: $(JAMBASE_NAME) $(JAMBASE_RESOURCES)
	$(MKDIRS) $(DSTROOT)$(INSTALL_PATH)
	$(CP) -p  $(SYMROOT)/$(JAMBASE_NAME) $(DSTROOT)$(INSTALL_PATH)
	($(CD) $(SRCROOT) ; $(CP) -p $(JAMBASE_RESOURCES) $(DSTROOT)$(INSTALL_PATH) )
	$(RM) -f $(DSTROOT)$(INSTALL_PATH)/Jambase    # for compatibility
	$(LN) -s $(JAMBASE_NAME) $(DSTROOT)$(INSTALL_PATH)/Jambase    # for compatibility
	$(CHMOD) -R $(INSTALL_PERMISSIONS) $(DSTROOT)$(INSTALL_PATH)
	$(CHOWN) -R $(INSTALL_AS_USER).$(INSTALL_AS_GROUP) $(DSTROOT)$(INSTALL_PATH)

installhdrs: install

installsrc:
	$(MKDIRS) $(SRCROOT)
	$(CP) -p $(JAMBASE_SOURCES) $(JAMBASE_RESOURCES) $(OTHER_SOURCES) $(SRCROOT)

clean:
	$(RM) -f $(SYMROOT)/$(JAMBASE_NAME)
