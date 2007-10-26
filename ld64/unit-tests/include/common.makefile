# stuff to include in every test Makefile

SHELL = /bin/sh

# set default to be host
ARCH ?= $(shell arch)

# set default to be all
VALID_ARCHS ?= "ppc ppc64 i386 x86_64"

MYDIR=$(shell cd ../../bin;pwd)

# if run within Xcode, add the just built tools to the command path
ifdef BUILT_PRODUCTS_DIR
	PATH := ${BUILT_PRODUCTS_DIR}:${MYDIR}:${PATH}
	COMPILER_PATH := ${BUILT_PRODUCTS_DIR}:${MYDIR}:${COMPILER_PATH}
else
	PATH := ${MYDIR}:${PATH}:
	COMPILER_PATH := ${MYDIR}:${COMPILER_PATH}:
endif
export PATH
export COMPILER_PATH

LD			= ld
OBJECTDUMP	= ObjectDump
MACHOCHECK	= machocheck
OTOOL = otool

CC		 = gcc-4.0 -arch ${ARCH}
CCFLAGS = -Wall -std=c99
ASMFLAGS =

CXX		  = g++-4.0 -arch ${ARCH}
CXXFLAGS = -Wall

RM      = rm
RMFLAGS = -rf

# utilites for Makefiles
PASS_IFF			= pass-iff-exit-zero.pl
PASS_IFF_SUCCESS	= ${PASS_IFF}
PASS_IFF_EMPTY		= pass-iff-no-stdin.pl
PASS_IFF_STDIN		= pass-iff-stdin.pl
FAIL_IFF			= fail-iff-exit-zero.pl
FAIL_IFF_SUCCESS	= ${FAIL_IFF}
PASS_IFF_ERROR		= pass-iff-exit-non-zero.pl
FAIL_IF_ERROR		= fail-if-exit-non-zero.pl
FAIL_IF_SUCCESS     = fail-if-exit-zero.pl
FAIL_IF_EMPTY		= fail-if-no-stdin.pl
FAIL_IF_STDIN		= fail-if-stdin.pl
PASS_IFF_GOOD_MACHO	= ${PASS_IFF} ${MACHOCHECK}
FAIL_IF_BAD_MACHO	= ${FAIL_IF_ERROR} ${MACHOCHECK}
FAIL_IF_BAD_OBJ		= ${FAIL_IF_ERROR} ${OBJECTDUMP} >/dev/null
