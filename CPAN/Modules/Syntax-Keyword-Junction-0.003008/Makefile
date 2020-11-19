NAME = Syntax-Keyword-Junction
VERSION = 0.003008

include ../Makefile.inc

# Remove existing (empty) auto/syntax directory, since we want to install
# auto/Syntax/Keyword/Junction, and the build system will get confused by
# two names that only differ by case-sensitiveness
patchafterconfigure:
	rm -rf $(DSTROOT)$(installarchlib)/auto/syntax
