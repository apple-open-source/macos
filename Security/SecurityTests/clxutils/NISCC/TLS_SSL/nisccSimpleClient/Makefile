# name of executable to build
EXECUTABLE=nisccSimpleClient
# C++ source (with .cpp extension)
CPSOURCE= nisccSimpleClient.cpp
# C source (.c extension)
CSOURCE=

#
# Differs from standard clxutils since ../ is not clxutils....
#
CL_UTIL_DIR=../../../

OUR_INCLUDES=-I$(CL_UTIL_DIR)/clAppUtils 

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
PROJ_LIBS=
#
# Optional lib search paths
#
PROJ_LIBPATH=
#
# choose one for cc
#
VERBOSE=
#VERBOSE=-v

#
# Other files to remove at 'make clean' time
#
OTHER_TO_CLEAN=

#
# non-standard frameworks (e.g., -framework foo)
#
PROJ_FRAMEWORKS= -framework CoreFoundation
#
# project-specific includes, with leading -I
#
PROJ_INCLUDES= 

#
# Optional C flags (warnings, optimizations, etc.)
#
PROJ_CFLAGS=

#
# Optional link flags (using cc, not ld)
#
PROJ_LDFLAGS=

#
# Optional dependencies
#
PROJ_DEPENDS=

#include ../Makefile.cdsa
############################################################################
# We just include our custom version of Makefile.cdsa intact right here....
############################################################################
#
# Common makefile fragment for dmitch's clxutils, nisccSimpleClient variant
# This is -included from project-specific Makefiles, assumed
# to be one directory down from this file.
#
# See Makefile.template for sample project-specific Makefile.
#

# Defaults for variables provided by buildit
# 
# Object files written to '.' unless OBJROOT specified
#
OBJROOT ?= .
SRCROOT ?= .
#
# Executables written to SYMROOT if specified, else to LOCAL_BUILD_DIR
# env var if specified, else to '.'.
#
SYMROOT ?= $(shell echo $(LOCAL_BUILD_DIR))
LOCAL_BUILD=$(SYMROOT)
ifeq "" "$(LOCAL_BUILD)"
	LOCAL_BUILD = .
endif

# independent of SYMROOT
CLEAN_DIR=$(shell echo $(LOCAL_BUILD_DIR))
ifeq "" "$(CLEAN_DIR)"
	CLEAN_DIR = .
endif

#
# DSTROOT only used for install
$
DSTROOT ?= ""

INSTALLDIR := $(DSTROOT)/usr/local/bin

CLAPP_UTILS= ../../..
UTIL_LIB_SRC= ../../../clAppUtils

OFILES= $(CSOURCE:%.c=$(OBJROOT)/%.o) $(CPSOURCE:%.cpp=$(OBJROOT)/%.o) 

#
# Assume final load with cc, not ld
#
STD_LIBS=-lclutils -lcsputils
STD_LIBPATH= -L$(LOCAL_BUILD) $(UTILLIB_PATH) -L$(UTIL_LIB_SRC) 
ALL_LIBS= $(STD_LIBS) $(PROJ_LIBS)
ALL_LIBPATHS= $(STD_LIBPATH) $(PROJ_LIBPATH) 
PRIV_FRAMEWORK_PATH= /System/Library/PrivateFrameworks

#
# Override this from the make command line to add e.g. -lMallocDebug
#
CMDLINE_LDFLAGS=

ALL_LDFLAGS= $(CMDLINE_LDFLAGS) $(ALL_LIBS) $(ALL_LIBPATHS) $(PROJ_LDFLAGS) \
	-F$(LOCAL_BUILD) -F$(PRIV_FRAMEWORK_PATH)

CC=c++

STD_FRAMEWORKS= -framework Security -framework CoreFoundation -framework security_cdsa_utils
ALL_FRAMEWORKS= $(STD_FRAMEWORKS) $(PROJ_FRAMEWORKS)

#
# to get to headers in frameworks
#
STD_FINCLUDES= -F$(LOCAL_BUILD) -F$(PRIV_FRAMEWORK_PATH)
#
# the common headers for csputils
#
STD_INCLUDES= -I$(CLAPP_UTILS) $(UTILLIB_HDRS) -F$(PRIV_FRAMEWORK_PATH) 
ALL_INCLUDES= $(STD_INCLUDES) $(PROJ_INCLUDES)
CINCLUDES= $(STD_FINCLUDES) $(ALL_INCLUDES)

###WFLAGS= -Wno-four-char-constants -Wall -Werror -Wno-format
WFLAGS= -Wno-four-char-constants -Wall  -Wno-format -Wno-deprecated-declarations

STD_CFLAGS= -g  $(VERBOSE) 
DEBUG_CFLAGS=

ALL_CFLAGS= $(CINCLUDES) $(STD_CFLAGS) $(PROJ_CFLAGS) $(WFLAGS) $(DEBUG_CFLAGS)

#
# Executable in build folder
#
BUILT_TARGET= $(LOCAL_BUILD)/$(EXECUTABLE)

first:	UTIL_LIB $(PROJ_DEPENDS) $(BUILT_TARGET)

build: first

install: build
	install -d -m 0755 $(INSTALLDIR)
	install -p -m 0755 $(BUILT_TARGET) $(INSTALLDIR)

installhdrs:

#
# Executable might be in . if no LOCAL_BUILD_DIR specified
#
clean:
	rm -f $(BUILT_TARGET) $(EXECUTABLE)
	cd $(SRCROOT); rm -f $(OFILES) *.o
	cd $(LOCAL_BUILD); rm -f $(EXECUTABLE) $(OTHER_TO_CLEAN)
	rm -f $(CLEAN_DIR)/$(EXECUTABLE)

UTIL_LIB:
	(cd $(UTIL_LIB_SRC); make)

$(BUILT_TARGET):	$(OFILES) 
	$(CC) -o $(BUILT_TARGET) $(ALL_FRAMEWORKS) $^ $(ALL_LDFLAGS)

$(OBJROOT)/%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $(OBJROOT)/$*.o $<

$(OBJROOT)/%.o: %.cpp
	$(CC) $(ALL_CFLAGS) -c -o $(OBJROOT)/$*.o $<

