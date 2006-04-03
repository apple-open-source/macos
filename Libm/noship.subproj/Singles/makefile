target:	Run

# Define ARCHS to be anything so that common.mk defines the
# multiple-architecture build rules rather than the single-architecture build
# rules.
ARCHS	= i386 ppc

include common.mk

# Enable LDLIBS below to build against locally built library.  Omit to build
# against system.
#LDLIBS	+= ../../build/libm_{i386,ppc}.a \
#	../../build/libmCommon_{i386,ppc}_static.a

# List built files to be removed when cleaning, in addition to .o et cetera.
AdditionalFilesToClean	= TestNan.{i386,ppc} TestNan_ld64.{c,ppc}

Run:	Run.$(shell arch)

Run.i386:	TestNan.i386
	@echo
	@echo "#-- Running i386 test programs. --"
	TestNan.i386

Run.ppc:	TestNan.ppc TestNan_ld64.ppc
	@echo
	@echo "#-- Running ppc test programs. --"
	TestNan.ppc
	TestNan_ld64.ppc

# Specify definitions to be used while building specific targets.
TestNan.i386:		ARCHS = i386
TestNan.ppc:		ARCHS = ppc
TestNan_ld64.ppc:	ARCHS = ppc
TestNan_ld64.ppc:	CFLAGS += -mlong-double-64
	# The _ld64 variant builds with 64-bit long-doubles.

# Make TestNan_ld64.c mirror TestNan.c.
TestNan_ld64.c:	TestNan.c
	ln -sf $^ $@
