# stuff to include in every test Makefile

SHELL = /bin/sh

# set default to be host
ARCH ?= $(shell arch)

# set default to be all
VALID_ARCHS ?= "ppc ppc64 i386 x86_64 armv6"

MYDIR=$(shell cd ../../bin;pwd)
LD			= ld
OBJECTDUMP	= ObjectDump
MACHOCHECK	= machocheck
OTOOL		= otool
REBASE		= rebase

ifdef BUILT_PRODUCTS_DIR
	# if run within Xcode, add the just built tools to the command path
	PATH := ${BUILT_PRODUCTS_DIR}:${MYDIR}:${PATH}
	COMPILER_PATH := ${BUILT_PRODUCTS_DIR}:${MYDIR}:${COMPILER_PATH}
	LD			= ${BUILT_PRODUCTS_DIR}/ld
	OBJECTDUMP	= ${BUILT_PRODUCTS_DIR}/ObjectDump
	MACHOCHECK	= ${BUILT_PRODUCTS_DIR}/machocheck
	REBASE		= ${BUILT_PRODUCTS_DIR}/rebase
else
	ifneq "$(findstring /unit-tests/test-cases/, $(shell pwd))" ""
		# if run from Terminal inside unit-test directory
		RELEASEDIR=$(shell cd ../../../build/Release;pwd)
		DEBUGDIR=$(shell cd ../../../build/Debug;pwd)
		PATH := ${RELEASEDIR}:${DEBUGDIR}:${MYDIR}:${PATH}
		COMPILER_PATH := ${RELEASEDIR}:${DEBUGDIR}:${MYDIR}:${COMPILER_PATH}
		LD			= ${RELEASEDIR}/ld
		OBJECTDUMP	= ${RELEASEDIR}/ObjectDump
		MACHOCHECK	= ${RELEASEDIR}/machocheck
		REBASE		= ${RELEASEDIR}/rebase
	else
		PATH := ${MYDIR}:${PATH}:
		COMPILER_PATH := ${MYDIR}:${COMPILER_PATH}:
	endif
endif
export PATH
export COMPILER_PATH


CC		 = gcc-4.2 -arch ${ARCH} ${SDKExtra}
CCFLAGS = -Wall -std=c99
ASMFLAGS =

CXX		  = g++-4.2 -arch ${ARCH} ${SDKExtra}
CXXFLAGS = -Wall

ifeq ($(ARCH),armv6)
	SDKExtra = -isysroot /Developer/SDKs/Extra
  LDFLAGS := -syslibroot /Developer/SDKs/Extra
  override FILEARCH = arm
else
  FILEARCH = $(ARCH)
endif

ifeq ($(ARCH),armv7)
	SDKExtra = -isysroot /Developer/SDKs/Extra
  LDFLAGS := -syslibroot /Developer/SDKs/Extra
  override FILEARCH = arm
else
  FILEARCH = $(ARCH)
endif

ifeq ($(ARCH),thumb)
	SDKExtra = -isysroot /Developer/SDKs/Extra
  LDFLAGS := -syslibroot /Developer/SDKs/Extra
  CCFLAGS += -mthumb
  CXXFLAGS += -mthumb
  override ARCH = armv6
  override FILEARCH = arm
else
  FILEARCH = $(ARCH)
endif

ifeq ($(ARCH),thumb2)
	SDKExtra = -isysroot /Developer/SDKs/Extra
  LDFLAGS := -syslibroot /Developer/SDKs/Extra
  CCFLAGS += -mthumb
  CXXFLAGS += -mthumb
  override ARCH = armv7
  override FILEARCH = arm
	CC = /Volumes/Leopard/Developer/Platforms/iPhoneOS.platform/usr/bin/gcc-4.2 -arch ${ARCH}
else
  FILEARCH = $(ARCH)
endif

RM      = rm
RMFLAGS = -rf

# utilites for Makefiles
PASS_IFF			= ${MYDIR}/pass-iff-exit-zero.pl
PASS_IFF_SUCCESS	= ${PASS_IFF}
PASS_IFF_EMPTY		= ${MYDIR}/pass-iff-no-stdin.pl
PASS_IFF_STDIN		= ${MYDIR}/pass-iff-stdin.pl
FAIL_IFF			= ${MYDIR}/fail-iff-exit-zero.pl
FAIL_IFF_SUCCESS	= ${FAIL_IFF}
PASS_IFF_ERROR		= ${MYDIR}/pass-iff-exit-non-zero.pl
FAIL_IF_ERROR		= ${MYDIR}/fail-if-exit-non-zero.pl
FAIL_IF_SUCCESS     = ${MYDIR}/fail-if-exit-zero.pl
FAIL_IF_EMPTY		= ${MYDIR}/fail-if-no-stdin.pl
FAIL_IF_STDIN		= ${MYDIR}/fail-if-stdin.pl
PASS_IFF_GOOD_MACHO	= ${PASS_IFF} ${MACHOCHECK}
FAIL_IF_BAD_MACHO	= ${FAIL_IF_ERROR} ${MACHOCHECK}
FAIL_IF_BAD_OBJ		= ${FAIL_IF_ERROR} ${OBJECTDUMP} >/dev/null
