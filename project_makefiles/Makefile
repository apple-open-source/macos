
.DEFAULT: all


# Turn on the use of the NeXT/Apple make hacks, for building the
# project_makefiles project itself.
USE_APPLE_PB_SUPPORT = all


# What OS are we building on?  
ifdef RC_OS
  PLATFORM_OS = $(RC_OS)
else
  PLATFORM_OS = nextstep
endif

# teflon and macos are really the same as nextstep - don't let anybody fool ya
ifeq ($(PLATFORM_OS), teflon)
  PLATFORM_OS = nextstep
endif

# merge nextstep and macos into a single value
ifeq ($(PLATFORM_OS), nextstep)
  MACLIKE = YES
endif
ifeq ($(PLATFORM_OS), macos)
  MACLIKE = YES
endif

MAKEFILEDIR = $(shell pwd)
include $(MAKEFILEDIR)/platform-variables.make

-include $(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/Makefiles/platform/makefile.platform

MAKEFILE_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_DIR)/Makefiles/project

ifeq ($(PLATFORM_OS), winnt)
   NEXT_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)/Utilities
   NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)
   EXEC_SUFFIX = .exe
   BIN_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)

   SUPPORT_PROGS = $(CLONEHDRS) $(CHANGES) $(ARCH_TOOL) $(OFILE_TOOL) $(LIBTOOL) $(FRAMEWORK_FLAGS)
   INSTALLED_TOOLS = changes.exe arch_tool.exe ofileListTool.exe frameworkFlags.exe
   SHELL   = $(NEXT_ROOT)$(SYSTEM_LIBRARY_EXECUTABLES_DIR)/sh
   RM     = $(NEXT_BIN)/rm
   CP     = $(NEXT_BIN)/cp
   ECHO   = $(NEXT_BIN)/echo
   MKDIRS = $(NEXT_BIN)/mkdirs
   TAR    = $(NEXT_BIN)/tar
   VERS_STRING = $(NEXTDEV_BIN)/vers_string
   SYMLINK = $(CP)
   STRIP = $(ECHO) Warning: Not stripping 
   CHMOD = $(NEXT_BIN)/chmod
else
   # assume general Unixy stuff
   SHELL = /bin/sh
   RM = /bin/rm
   CP = /bin/cp -p
   SYMLINK = /bin/ln -s
   ECHO = /bin/echo
   TAR = /usr/bin/gnutar
   CHMOD = /bin/chmod

 ifeq ($(MACLIKE), YES)
   BIN_INSTALL_DIR = $(DSTROOT)/usr/lib
   SUPPORT_PROGS = $(FASTCP) $(CLONEHDRS) $(CHANGES) $(ARCH_TOOL) $(OFILE_TOOL) $(FRAMEWORK_FLAGS)
   INSTALLED_TOOLS = fastcp clonehdrs changes arch_tool ofileListTool frameworkFlags
   STRIP = /usr/bin/strip
   CC = /usr/bin/cc
   MIG = $(NEXT_ROOT)/usr/bin/mig
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
   VERS_STRING = /usr/bin/vers_string
ifneq "" "$(wildcard /usr/etc/chown)"
  CHOWN = /usr/etc/chown
else
  CHOWN = /usr/sbin/chown
endif
ifneq "" "$(wildcard /bin/chgrp)"
  CHGRP = /bin/chgrp
else
  CHGRP = /usr/bin/chgrp
endif
   CHOWN_TO_ROOT = $(CHOWN) -fR root
   CHGRP_TO_BIN = $(CHGRP) -fR wheel

 endif

 ifeq ($(PLATFORM_OS), solaris)
   MAKEFILE_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_DIR)/Makefiles/project
   BIN_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
   SUPPORT_PROGS = $(FASTCP) $(CLONEHDRS) $(CHANGES) $(ARCH_TOOL) $(OFILE_TOOL) $(LIBTOOL) $(INSTALLTOOL) $(FRAMEWORK_FLAGS)
   INSTALLED_TOOLS = fastcp clonehdrs changes arch_tool ofileListTool frameworkFlags
   NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
   SUNDEV_BIN = /usr/ccs/bin
   CC = $(NEXTDEV_BIN)/gcc
   MKDIRS = /usr/bin/mkdir -p
   VERS_STRING = $(NEXT_ROOT)/usr/local/bin/vers_string
   STRIP = $(SUNDEV_BIN)/strip
   CHGRP_TO_BIN = /bin/chgrp -fR bin
   CHOWN_TO_ROOT = /usr/ucb/chown -fR root
 endif

 ifeq ($(PLATFORM_OS), hpux)
   MAKEFILE_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_DIR)/Makefiles/project
   BIN_INSTALL_DIR = $(DSTROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
   SUPPORT_PROGS = $(FASTCP) $(CLONEHDRS) $(CHANGES) $(ARCH_TOOL) $(OFILE_TOOL) $(LIBTOOL) $(INSTALLTOOL) $(FRAMEWORK_FLAGS)
   INSTALLED_TOOLS = fastcp clonehdrs changes arch_tool ofileListTool frameworkFlags
   NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
   CC = $(NEXTDEV_BIN)/gcc
   MKDIRS = /usr/bin/mkdir -p
   VERS_STRING = $(NEXT_ROOT)/usr/local/bin/vers_string
   STRIP = /bin/strip
   CHGRP_TO_BIN = /bin/chgrp -R bin
   CHOWN_TO_ROOT = /bin/chown -R root
 endif
endif


INSTALL_DIRS = $(MAKEFILE_INSTALL_DIR) $(BIN_INSTALL_DIR) 

TEMPLATES = Makefile.preamble.template Makefile.postamble.template

MAKEFILES = app.make framework.make bundle.make tool.make \
		library.make aggregate.make subproj.make \
		bundle-common.make common.make basicrules.make \
		platform-variables.make

OS_SPECIFIC_MAKEFILES = nextstep-specific.make macos-specific.make winnt-specific.make \
	       		    solaris-specific.make hpux-specific.make

ifeq ($(PLATFORM_OS), winnt)
OBJROOT = c:/tmp/Objects/PM
SYMROOT = c:/tmp/Objects/PM
else
OBJROOT = $(HOME)/Objects/PM
SYMROOT = $(HOME)/Objects/PM
endif
DSTROOT = $(NEXT_ROOT)
OFILE_DIR = $(OBJROOT)/$(PLATFORM_OS)_obj

CFLAGS = -g -O -Wmost -I. -I$(OFILE_DIR) $(OTHER_CFLAGS) $(RC_CFLAGS) 

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $(OFILE_DIR)/$*.o

.m.o:
	$(CC) $(CFLAGS) -F$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks -c $*.m -o $(OFILE_DIR)/$*.o

VPATH = $(OFILE_DIR)

CLONEHDRS_CFILES = clonehdrs.c 
CLONEHDRS_OFILES = $(CLONEHDRS_CFILES:.c=.o)
CLONEHDRS_SRC = $(CLONEHDRS_CFILES)
CLONEHDRS_LIBS = 
CLONEHDRS = $(SYMROOT)/clonehdrs$(EXEC_SUFFIX)

ifeq ($(MACLIKE), YES)
   FASTCP_CFILES = fastcp.c publicizeCopy.c 
   FASTCP_OFILES = $(FASTCP_CFILES:.c=.o) makeUser.o
   FASTCP_HFILES = make_defs.h 
   FASTCP_SRC = $(FASTCP_CFILES) $(FASTCP_CLASSES) $(FASTCP_HFILES) make.defs
   FASTCP_LIBS =
else
   FASTCP_CFILES = fastcp.c publicizeCopy.c
   FASTCP_OFILES = $(FASTCP_CFILES:.c=.o)
   FASTCP_HFILES = 
   FASTCP_SRC = $(FASTCP_CFILES) $(FASTCP_CLASSES) $(FASTCP_HFILES)
   FASTCP_LIBS = 
endif
FASTCP = $(SYMROOT)/fastcp$(EXEC_SUFFIX)

CHANGES_CFILES = changes.c
CHANGES_OFILES = changes.o
CHANGES_SRC = $(CHANGES_CFILES) 
CHANGES_LIBS = 
CHANGES = $(SYMROOT)/changes$(EXEC_SUFFIX)

ARCH_TOOL_CFILES = arch_tool.c
ARCH_TOOL_OFILES = $(ARCH_TOOL_CFILES:.c=.o)
ARCH_TOOL_SRC = $(ARCH_TOOL_CFILES) 
ARCH_TOOL_LIBS = 
ARCH_TOOL = $(SYMROOT)/arch_tool$(EXEC_SUFFIX)

OFILE_TOOL_CFILES = ofileListTool.c
OFILE_TOOL_OFILES = $(OFILE_TOOL_CFILES:.c=.o)
OFILE_TOOL_SRC = $(OFILE_TOOL_CFILES) 
OFILE_TOOL_LIBS = 
OFILE_TOOL = $(SYMROOT)/ofileListTool$(EXEC_SUFFIX)

FRAMEWORK_FLAGS_CFILES = frameworkFlags.c
FRAMEWORK_FLAGS_OFILES = $(FRAMEWORK_FLAGS_CFILES:.c=.o)
FRAMEWORK_FLAGS_SRC = $(FRAMEWORK_FLAGS_CFILES) 
FRAMEWORK_FLAGS_LIBS = 
FRAMEWORK_FLAGS = $(SYMROOT)/frameworkFlags$(EXEC_SUFFIX)

LIBTOOL = $(SYMROOT)/libtool
LIBTOOL_SRC = libtool.winnt libtool.solaris libtool.hpux

INSTALLTOOL = $(SYMROOT)/installtool
INSTALLTOOL_SRC = installtool.pdo

ifeq ($(MACLIKE), YES)
publicizeCopy.c: $(OFILE_DIR)/make.h
makeUser.c: $(OFILE_DIR)/make.h
$(OFILE_DIR)/make.h $(OFILE_DIR)/makeUser.c: ./make.defs
	$(CP) ./make.defs $(OFILE_DIR)
	(cd $(OFILE_DIR) ; $(MIG) -untyped make.defs ; $(RM) -f makeServer.c)
endif

$(FASTCP):	$(FASTCP_OFILES) $(FASTCP_HFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FASTCP_OFILES) $(FASTCP_LIBS)

$(CHANGES): $(CHANGES_OFILES) $(CHANGES_HFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CHANGES_OFILES) $(CHANGES_LIBS)

$(OFILE_TOOL): $(OFILE_TOOL_OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OFILE_TOOL_OFILES) $(OFILE_TOOL_LIBS)

$(ARCH_TOOL): $(ARCH_TOOL_OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(ARCH_TOOL_OFILES) $(ARCH_TOOL_LIBS)

$(CLONEHDRS): $(CLONEHDRS_OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CLONEHDRS_OFILES) $(CLONEHDRS_LIBS)

$(LIBTOOL): libtool.$(PLATFORM_OS) $(OFILE_DIR)
	$(CP) libtool.$(PLATFORM_OS) $(LIBTOOL)
	$(CHMOD) +x $(LIBTOOL)
	$(CHMOD) u+w $(LIBTOOL)

$(INSTALLTOOL): installtool.pdo $(OFILE_DIR)
	$(CP) installtool.pdo $(INSTALLTOOL)
	$(CHMOD) +x $(INSTALLTOOL)
	$(CHMOD) u+w $(INSTALLTOOL)

$(FRAMEWORK_FLAGS): $(FRAMEWORK_FLAGS_OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FRAMEWORK_FLAGS_OFILES) $(FRAMEWORK_FLAGS_LIBS)

SOURCES = PB.project $(MAKEFILES) $(OS_SPECIFIC_MAKEFILES) $(TEMPLATES) \
          $(FASTCP_SRC) $(CHANGES_SRC) $(ARCH_TOOL_SRC) $(OFILE_TOOL_SRC) \
	   $(CLONEHDRS_SRC) $(FRAMEWORK_FLAGS_SRC) $(LIBTOOL_SRC) \
	   $(INSTALLTOOL_SRC) $(WINDOWS_CMDS)

######################################


all: $(OFILE_DIR) $(SYMROOT) $(SUPPORT_PROGS)

clean:	
	$(RM) -rf *~ make.h makeUser.c $(SUPPORT_PROGS) *.exe *obj

$(SRCROOT) $(INSTALL_DIRS) $(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

install:: installhdrs clean_first all $(INSTALL_DIRS) $(SUPPORT_PROGS)
	$(CP) $(SUPPORT_PROGS) $(BIN_INSTALL_DIR)
	@(for t in $(INSTALLED_TOOLS) ; do \
		cmd="$(STRIP) $(BIN_INSTALL_DIR)/$$t" ; \
		$(ECHO) $$cmd ; $$cmd ; \
	done)
	$(CP) $(MAKEFILES) $(PLATFORM_OS)-specific.make $(TEMPLATES) \
		$(MAKEFILE_INSTALL_DIR)
	(cd $(MAKEFILE_INSTALL_DIR) ; \
	 $(SYMLINK) bundle.make palette.make ; \
	 $(ECHO) "PLATFORM_OS = $(PLATFORM_OS)" > platform.make; \
	 $(ECHO) 'include $$(MAKEFILEPATH)/project/platform-variables.make' >> platform.make)
	$(ECHO) `$(VERS_STRING) -n` > $(MAKEFILE_INSTALL_DIR)/VERSION
ifdef CHMOD
	-$(CHMOD) -R ugo-w $(BIN_INSTALL_DIR)
	-$(CHMOD) -R ugo-w $(MAKEFILE_INSTALL_DIR)
endif
ifneq "$(CHOWN_TO_ROOT)" ""
	$(CHOWN_TO_ROOT) $(BIN_INSTALL_DIR)
	$(CHOWN_TO_ROOT) $(MAKEFILE_INSTALL_DIR)
endif
ifneq "$(CHGRP_TO_BIN)" ""
	$(CHGRP_TO_BIN) $(BIN_INSTALL_DIR)
	$(CHGRP_TO_BIN) $(MAKEFILE_INSTALL_DIR)
endif


installhdrs::
	echo "No headers to install"

clean_first:
	-$(RM) -rf $(MAKEFILE_INSTALL_DIR)

installsrc:: $(SRCROOT)
	$(TAR) cf - Makefile $(SOURCES) | (cd ${SRCROOT}; $(TAR) xfp -)

