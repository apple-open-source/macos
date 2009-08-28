########################################################################################
#                                                                                      #
#                                       Makefile                                       #
#                                                                                      #
#                       Makefile to build the gdb MacsBug plugin                       #
#                                                                                      #
#                                     Ira L. Ruben                                     #
#                      Copyright Apple Computer, Inc. 2000-2006                        #
#                                                                                      #
########################################################################################

# This Makefile can be used explicitly with make, or it can be used to install the
# MacsBug plugin with buildit.

# When used explicitly with make the following targets are available:

# all		Build the entire MacsBug plugin tree and its gdb plugin API.
# clean		Clean both the MacsBug and API objects and plugin.
# cleanmb	Clean only the MacsBug objects and plugin.

# The floowing defines may be set on the make line:

# VERSION	The version number of the MacsBug plugin (used for the DV command).

# SRCROOT	Which directory the MacsBug sources are in (default is .).

# SYMROOT	Target directory to contain plugin (MacsBug, default is .).

# OBJROOT	Directory to contain the Objects/ directory, i.e., where .o's go,
#		default is ./Objects).

# API_DIR	Directory containing the API sources and it's Makefile.  Currently
#		the default is SRCROOT, i.e., the API is a directory located within
#		the Macsbug directory.  If the API is made a separate project this
#		will change.

# When this Makefile is used with buildit BUILDIT_DIR should be set to where buildit
# is to create its output.  Buildit expects and uses the following targets:

# installsrc	Installs all the source and additional files for the build into
#		SRCROOT.  Use buildit's -noinstallsrc to suppress the source copy.

# clean		Clean both the MacsBug and API objects and plugin.  Use buildit's
#		-noclean to suppress the clean step.

# installhdrs	Does nothing since there are no headers to install.  Buildit's
#		-noinstallhdrs causes this step to be skipped.

# install	Installs the MacsBug plugin and assocated files into DSTROOT which is
#		defined by buildit.  Currently the following layout describes where the
#		default install places the files:

# 		/usr/libexec/gdb/plugins/MacsBug/
#     			   	   		 MacsBug_plugin
#     			   	   		 gdbinit-MacsBug-without-plugin
#     			   	   		 gdbinit-MacsBug
#     			   	   		 README.txt
#     			   	   		 README.html

# As a reminder here some other possibly "useful" buildit options: -noverify, -nosum.

#--------------------------------------------------------------------------------------#

PROJECT				=	MacsBug
PLUGIN				=       MacsBug_plugin
VERSION				= 	1.6

SRCROOT				=	.
SYMROOT				=	.
OBJROOT				=	.
OFILE_DIR			=	$(OBJROOT)/Objects
API_DIR				=	$(SRCROOT)

GDBROOT				=	$(GDB_BUILD_ROOT)
GDBDIR				=	/usr/libexec/gdb
IDIR				=	$(GDBDIR)/plugins/$(PROJECT)
DSTDIR				=	$(DSTROOT)$(IDIR)

PLUGIN_SUPPORT			= 	$(API_DIR)/gdb_plugin_support

BUILD_VERSION			=	10.4

ifndef BINUTILS_BUILD_ROOT
BINUTILS_BUILD_ROOT = $(SDKROOT)
endif

ifndef GDB_BUILD_ROOT
GDB_BUILD_ROOT = $(SDKROOT)
endif

# The following defines $(gdb_plugin_support.o) as a function of using the Makefile
# by buildit or using it explicitly.  Only buildit will use the "install" target.  For
# that we want the API library in the SYMROOT.  For explict builds we leave the library
# in the API's own directory.

ifeq ($(filter install,$(MAKECMDGOALS)),install)
gdb_plugin_support.o		=	$(SYMROOT)/gdb_plugin_support.o
else
gdb_plugin_support.o		=	$(PLUGIN_SUPPORT)/gdb_plugin_support.o
endif

CC				=	cc

VPATH				=	$(OFILE_DIR)

GDBINIT				=	gdbinit-MacsBug
OLD_GDBINIT			=	gdbinit-MacsBug-without-plugin

CFILES				=	MacsBug_plugins.c		\
					MacsBug_utils.c			\
					MacsBug_cmdline.c		\
					MacsBug_display.c		\
					MacsBug_patches.c		\
					MacsBug_set.c			\
					MacsBug_screen.c

HFILES				= 	MacsBug.h

HEADERS				=	$(HFILES:%=$(SRCROOT)/%) 	\
					$(PLUGIN_SUPPORT)/gdb.h

INCLUDES 			= 	-I$(PLUGIN_SUPPORT) -I.

OPT				=	-Os

CFLAGS 				= 	-gdwarf-2			\
					$(OPT)				\
					-c				\
					-DVERSION=\""$(VERSION)"\"	\
					$(INCLUDES)

ifneq ($(BUILD_VERSION),10.0)
LDFLAGS				=	-bundle 			\
					-bundle_loader $(GDBROOT)$(GDBDIR)/gdb-powerpc-apple-darwin \
					-twolevel_namespace \
					#-F$(BINUTILS_BUILD_ROOT)/System/Library/PrivateFrameworks -framework liberty
else
LDFLAGS				=	-bundle 			\
					-undefined suppress
endif

INSTALLSRC_FILES		=	$(CFILES)			\
					MacsBug_testing.i		\
					$(HFILES)			\
					Makefile			\
					README.txt			\
					README.html			\
					$(GDBINIT)			\
					$(OLD_GDBINIT)

OBJS				= 	$(CFILES:%.c=$(OFILE_DIR)/%.o)

INSTALL_FILES_PARTIAL_LIST	=	README.txt			\
					README.html			\
					$(OLD_GDBINIT)

#INSTALL_FILES			=	$(INSTALL_FILES_PARTIAL_LIST:%=$(SRCROOT)/%)

# The following is for my (Ira) own private builds.  I use a separate directory for
# the set of MacsBug sources I'm currently working on and my expiremental plugin(s).
# But I still use the system gdb.  Therefore I need to add an extra line to the link
# step to setgid and change the group to procmod.  Sigh :-(                     Ira

PRIVATE_BUILD			=	
ifneq ($(PRIVATE_BUILD),,)
FIXUP 				=	
else
FIXUP				=	
endif

# Note: Alternative FIXUP:
#FIXUP 				=	dsymutil -o $(SYMROOT)/$(PLUGIN).dSYM $(SYMROOT)/$(PLUGIN); \
#					strip -x $(SYMROOT)/$(PLUGIN)
#					

# While this mimics the full gdb build, for my private builds, I do not see any need
# to do this since gdb is fully capable of using the embedded dwarf debug info (even
# if it does save a few disk bytes).

#--------------------------------------------------------------------------------------#
#                       *----------------*
#                       | Default Target |
#                       *----------------*

all				: 	$(OFILE_DIR)  			\
					$(SYMROOT)			\
					$(SYMROOT)/$(PLUGIN)

#--------------------------------------------------------------------------------------#
#                       *-----------------*
#                       | Buildit Targets |
#                       *-----------------*

installsrc			:
	mkdir -p  $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALLSRC_FILES) | (cd $(SRCROOT) && gnutar xf -)
	chmod 444 $(SRCROOT)/*
	$(MAKE) -C gdb_plugin_support SRCROOT=$(SRCROOT)/gdb_plugin_support installsrc

clean				:
	rm -rf $(OFILE_DIR) $(PLUGIN) $(gdb_plugin_support.o)
	cd $(PLUGIN_SUPPORT) && make clean SRCROOT=$(PLUGIN_SUPPORT)

installhdrs			:	

install				:	all $(DSTDIR)
	install -p -m 644 README.txt README.html $(OLD_GDBINIT) $(DSTDIR)
	#chmod 644 $(DSTDIR)/README.txt $(DSTDIR)/README.html $(DSTDIR)/$(OLD_GDBINIT)
	install -p -m 644 $(GDBINIT) "$(DSTDIR)/$(GDBINIT)"
	#chmod 644 $(DSTDIR)/$(GDBINIT)
	dsymutil -o $(SYMROOT)/$(PLUGIN).dSYM $(SYMROOT)/$(PLUGIN)
	strip -x $(SYMROOT)/$(PLUGIN) -o $(DSTDIR)/$(PLUGIN)
	install -m 755 $(DSTDIR)/$(PLUGIN) $(SYMROOT)/$(PLUGIN)
	#chmod 755 $(DSTDIR)/$(PLUGIN)

#--------------------------------------------------------------------------------------#
#                        *-------------*
#                        | Build Rules |
#                        *-------------*

$(SYMROOT)/$(PLUGIN) 		: 	gdb_plugin_support.o $(OBJS) 
	$(CC) $(LDFLAGS) $(OBJS) -arch ppc $(gdb_plugin_support.o) -o $(SYMROOT)/$(PLUGIN)
	-$(FIXUP)

$(OFILE_DIR)/MacsBug_utils.o 	: 	MacsBug_utils.c MacsBug_testing.i 	\
					$(HFILES) 				\
					$(PLUGIN_SUPPORT)/gdb.h
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(OFILE_DIR)/MacsBug_utils.o MacsBug_utils.c 

$(OFILE_DIR)/%.o		:	%.c					\
					$(HFILES)				\
					$(PLUGIN_SUPPORT)/gdb.h
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(OFILE_DIR)/$(notdir $*).o $<

$(OFILE_DIR) $(SYMROOT) $(DSTDIR):
	mkdir -p $@

gdb_plugin_support.o 		: 	force
	cd $(PLUGIN_SUPPORT) && make SRCROOT=$(PLUGIN_SUPPORT) OPT=$(OPT)

force				:

cleanmb				:
	rm -rf $(OFILE_DIR) $(PLUGIN)

#--------------------------------------------------------------------------------------#
#                        *---------------*
#                        | Private Rules |
#                        *---------------*

debug				:
	$(MAKE) OPT=-O0 PRIVATE_BUILD=1

private				:
	$(MAKE) PRIVATE_BUILD=1

#--------------------------------------------------------------------------------------#
