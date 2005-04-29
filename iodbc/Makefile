##
# Makefile for iODBC
##

# Configured for Panther (10.3) by default
# To rebuild on Jaguar (10.2) you need to define JAGUAR
#Extra_CC_Flags = -DJAGUAR

# Project info
Project  = iodbc
UserType = Developer
ToolType = Commands
#Install_Prefix = $(USRDIR)

GnuAfterInstall = fixup

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# building in ~rc has to avoid ~ as a separator since it's used in pathnames
export LIBTOOL_CMD_SEP = +

# For building fat
export MACOSX_DEPLOYMENT_TARGET = 10.4
export LD_TWOLEVEL_NAMESPACE = 1

Install_Target = install
Extra_Configure_Flags += --with-iodbc-inidir=/Library/ODBC --disable-gui TMPDIR=$(OBJROOT)
CFLAGS += -Wl,-framework,CoreFoundation

fixup:
	@echo "Trashing RC undesirables *.la files"; \
	${RM} $(RC_Install_Prefix)/lib/*.la; \
	${STRIP} -S $(RC_Install_Prefix)/lib/libiodbc.a; \
	${STRIP} -S $(RC_Install_Prefix)/lib/libiodbcinst.a; \
	${STRIP} -S $(RC_Install_Prefix)/lib/libiodbc.2.dylib; \
	${STRIP} -S $(RC_Install_Prefix)/lib/libiodbcinst.2.dylib; \
	${STRIP} -S $(RC_Install_Prefix)/bin/iodbctest; \
	${STRIP} -S $(RC_Install_Prefix)/bin/iodbctestw; \

	${MKDIR} $(RC_Install_Prefix)/local/OpenSourceVersions; \
	${CP} $(SRCROOT)/iodbc.plist $(RC_Install_Prefix)/local/OpenSourceVersions/; \
	${CHOWN} root:wheel $(RC_Install_Prefix)/local/OpenSourceVersions/iodbc.plist; \
	${CHMOD} 644 $(RC_Install_Prefix)/local/OpenSourceVersions/iodbc.plist; \

	${MKDIR} $(RC_Install_Prefix)/local/OpenSourceLicenses; \
	${CP} $(SRCROOT)/iodbc.txt $(RC_Install_Prefix)/local/OpenSourceLicenses/; \
	${CHOWN} root:wheel $(RC_Install_Prefix)/local/OpenSourceLicenses/iodbc.txt; \
	${CHMOD} 644 $(RC_Install_Prefix)/local/OpenSourceLicenses/iodbc.txt; \
