# xotclConfig.sh --
# 
# This shell script (for sh) is generated automatically by XOTcl's
# configure script.  It will create shell variables for most of
# the configuration options discovered by the configure script.
# This script is intended to be included by the configure scripts
# for XOTcl extensions so that they don't have to figure this all
# out for themselves.  This file does not duplicate information
# already provided by tclConfig.sh, so you may need to use that
# file in addition to this one.
#
# The information in this file is specific to a single platform.

# XOTcl's version number.
XOTCL_VERSION='1.2'
XOTCL_MAJOR_VERSION='1'
XOTCL_MINOR_VERSION='2'
XOTCL_RELEASE_LEVEL='.0'

# The name of the XOTcl library (may be either a .a file or a shared library):
XOTCL_LIB_FILE=libxotcl1.2.so

# String to pass to linker to pick up the XOTcl library from its
# build directory.
XOTCL_BUILD_LIB_SPEC='-L/home/neumann/xotcl-1.2.0/unix -lxotcl1.2'

# String to pass to linker to pick up the XOTcl library from its
# installed directory.
XOTCL_LIB_SPEC='-L/usr/lib -lxotcl1.2'

# The name of the XOTcl stub library (a .a file):
XOTCL_STUB_LIB_FILE=libxotclstub1.2.a

# String to pass to linker to pick up the XOTcl stub library from its
# build directory.
XOTCL_BUILD_STUB_LIB_SPEC='-L/home/neumann/xotcl-1.2.0/unix -lxotclstub1.2'

# String to pass to linker to pick up the XOTcl stub library from its
# installed directory.
XOTCL_STUB_LIB_SPEC='-L/usr/lib -lxotclstub1.2'

# Name of the xotcl stub library with full path in build and install directory
XOTCL_BUILD_STUB_LIB_PATH='/home/neumann/xotcl-1.2.0/unix/libxotclstub1.2.a'
XOTCL_STUB_LIB_PATH='/usr/lib/libxotclstub1.2.a'

# Location of the top-level source directories from which XOTcl
# was built.  This is the directory that contains generic, unix, etc.
# If XOTcl was compiled in a different place than the directory
# containing the source files, this points to the location of the sources,
# not the location where XOTcl was compiled.
XOTCL_SRC_DIR='/home/neumann/xotcl-1.2.0/unix/..'

# shared and unshared library suffix
XOTCL_SHARED_LIB_SUFFIX=1.2.so
XOTCL_UNSHARED_LIB_SUFFIX=1.2.a

