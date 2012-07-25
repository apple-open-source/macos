#
# Makefile for iODBC
##

# Configured for Panther (10.5) by default
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
export MACOSX_DEPLOYMENT_TARGET = 10.8
export LD_TWOLEVEL_NAMESPACE = 1
# We want to create dSYM files in fixup, which means we can't strip until then; so make
# strip a no-op.
export STRIP = /bin/echo
STRIP2 = /usr/bin/strip
LIPO = /usr/bin/lipo

Install_Target = install
Extra_Configure_Flags += --prefix=/usr --with-iodbc-inidir=/Library/ODBC --disable-gui TMPDIR=$(OBJROOT)
CFLAGS += -Wl,-framework,CoreFoundation

# The first half of fixup is a hack to get dsymutil generating (correct) .dSYM files.
# When dsymutil runs, it expects .o files used during linking to be laid out in the filesystem 
# in the same way they were when a library was linked. Unfortunately, the iodbc build process 
# puts things in lib*.lax/lib*.a directories during linking, and then removes those directories,
# which results in incomplete .dSYM files. 
# Rather than try to change the actual build process, this just reconstructs the filesystem 
# layout to make dsymutil happy.
fixup:
	${MKDIR} "$(OBJROOT)/iodbc/.libs/libiodbc.lax/libiodbctrace.a";\
	${MKDIR} "$(OBJROOT)/iodbc/.libs/libiodbc.lax/libiodbc_common.a";\
	${CP} $(OBJROOT)/iodbc/trace/*.o $(OBJROOT)/iodbc/.libs/libiodbc.lax/libiodbctrace.a/.;\
	${CP} $(OBJROOT)/iodbcinst/*.o $(OBJROOT)/iodbc/.libs/libiodbc.lax/libiodbc_common.a/.;\
	${MKDIR} "$(OBJROOT)/iodbcinst/.libs/libiodbcinst.lax/libiodbc_common.a";\
	${CP} $(OBJROOT)/iodbcinst/*.o $(OBJROOT)/iodbcinst/.libs/libiodbcinst.lax/libiodbc_common.a/.;\
	${CP} "$(RC_Install_Prefix)/bin/iodbctest" "$(SYMROOT)/iodbctest"; \
	${CP} "$(RC_Install_Prefix)/bin/iodbctestw" "$(SYMROOT)/iodbctestw"; \
	${CP} "$(RC_Install_Prefix)/lib/libiodbc.2.1.18.dylib" "$(SYMROOT)/libiodbc.2.1.18.dylib"; \
	$(CP) "$(OBJROOT)/iodbcinst/.libs/libiodbcinst.2.1.18.dylib" "$(SYMROOT)/libiodbcinst.2.1.18.dylib"; \
	$(CP) "$(OBJROOT)/iodbcinst/.libs/libiodbcinst.2.1.18.dylib.dSYM" "$(SYMROOT)/libiodbcinst.2.1.18.dylib.dSYM"; \
	${RM} $(RC_Install_Prefix)/lib/*.la; \
	${STRIP2} -S $(RC_Install_Prefix)/lib/libiodbc.a; \
	${STRIP2} -S $(RC_Install_Prefix)/lib/libiodbcinst.a; \
	${STRIP2} -S $(RC_Install_Prefix)/lib/libiodbc.2.dylib; \
	${STRIP2} -S $(RC_Install_Prefix)/lib/libiodbcinst.2.dylib; \
	${STRIP2} -S $(RC_Install_Prefix)/bin/iodbctest; \
	${LIPO} -thin x86_64 $(RC_Install_Prefix)/bin/iodbctest -output $(RC_Install_Prefix)/bin/tmp; \
	${MV} $(RC_Install_Prefix)/bin/tmp $(RC_Install_Prefix)/bin/iodbctest; \
	${STRIP2} -S $(RC_Install_Prefix)/bin/iodbctestw; \
	${LIPO} -thin x86_64 $(RC_Install_Prefix)/bin/iodbctestw -output $(RC_Install_Prefix)/bin/tmp; \
	${MV} $(RC_Install_Prefix)/bin/tmp $(RC_Install_Prefix)/bin/iodbctestw; \

	${MKDIR} $(RC_Install_Prefix)/local/OpenSourceVersions; \
	${CP} $(SRCROOT)/iodbc.plist $(RC_Install_Prefix)/local/OpenSourceVersions/; \
	${CHOWN} root:wheel $(RC_Install_Prefix)/local/OpenSourceVersions/iodbc.plist; \
	${CHMOD} 644 $(RC_Install_Prefix)/local/OpenSourceVersions/iodbc.plist; \

	${MKDIR} $(RC_Install_Prefix)/local/OpenSourceLicenses; \
	${CP} $(SRCROOT)/iodbc.txt $(RC_Install_Prefix)/local/OpenSourceLicenses/; \
	${CHOWN} root:wheel $(RC_Install_Prefix)/local/OpenSourceLicenses/iodbc.txt; \
	${CHMOD} 644 $(RC_Install_Prefix)/local/OpenSourceLicenses/iodbc.txt; \
