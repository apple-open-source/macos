##
# CoreOS Makefile for unzip
##

Project		= unzip
UserType	= Developer
ToolType	= Commands

GnuNoConfigure	= YES
GnuNoBuild	= YES

GnuAfterInstall	= destroot ossinfo

OSLICENSES     = /usr/local/OpenSourceLicenses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= -f unix/Makefile macosx
Extra_CC_Flags	= -mdynamic-no-pic
CC_Optimize	=	# project handles this
Extra_Install_Flags	=	LOC="$(CC_Flags)" LD="$(CC) $(LDFLAGS)"

build:: install_source_local

destroot:
	$(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) \
		-f unix/Makefile install \
		MANDIR=$(RC_Install_Man)/'man$$(manext)'

ossinfo:
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(Project).txt

install_source_local:
	cd $(Project) && $(PAX) -rw . $(BuildDirectory)
