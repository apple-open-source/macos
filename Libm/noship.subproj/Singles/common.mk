.PHONY:	TargetError
TargetError:
	@echo "Specify a target before including common.mk."


DependencyFlags	= -MMD
CommonFlags		= \
	-force_cpusubtype_ALL \
	-malign-natural \
	-Wmost \
	-pedantic \
	$(DependencyFlags)
	# -Wmost is equivalent to -Wall -Wno-parentheses (Apple-specific).
	#
	# Try -fnew-ra, new graph-coloring register allocator.
	#
	# Use -faltivec when using AltiVec extensions.
	#
	# Might need to remove -fstrict-aliasing when aliasing memory, as in
	# test program that works with raw memory.

ifdef DEBUG
DebugFlags		= -g
ProductionFlags	= -O3 -funroll-loops -fstrict-aliasing
else
DebugFlags		= -g
ProductionFlags	= -O3 -funroll-loops -fstrict-aliasing
endif

CAndCXXFlags	= $(CommonFlags) $(ProductionFlags) $(DebugFlags)

ASFLAGS			= -force_cpusubtype_ALL ${DebugFlags} $(DependencyFlags)
CFLAGS			= $(CAndCXXFlags) -std=c99
CXXFLAGS		= $(CAndCXXFlags) -std=c++98
LDFLAGS			= $(DebugFlags)

#LDLIBS			+=


# Convenience target to remove intermediate files.
# (AdditionalFilesToClean provides a way to remove created executable files,
# since they do not have a distinctive name pattern.)
.PHONY:	clean
clean:
	@echo
	@echo "#-- Removing recreatable files. --"
	rm -f *.o *.d *.a $(AdditionalFilesToClean)


# Include any dependency files in the directory.
-include *.d


# The standard build rules are replaced below
#
# When building just for the native architecture, these build rules differ
# from the standard rules primarily in that they label what they are doing.
#
# When building for multiple architectures, separate object files are produced
# and then combined into fat files.  This is necessary to use the -M GCC
# switch to produce dependency files.
#
# We expect make to provide the following definitions by default:
#
#COMPILE.S		= $(CC) $(ASFLAGS) $(CPPFLAGS) $(TARGET_MACH) -c
#COMPILE.c		= $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
#COMPILE.cc		= $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
#COMPILE.cpp	= $(COMPILE.cc)
#COMPILE.f		= $(FC) $(FFLAGS) $(TARGET_ARCH) -c
#LINK.S			= $(CC) $(ASFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_MACH)
#LINK.c			= $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
#LINK.cc		= $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
#LINK.cpp		= $(LINK.cc)
#LINK.f			= $(FC) $(FFLAGS) $(LDFLAGS) $(TARGET_ARCH)
#LINK.o			= $(CC) $(LDFLAGS) $(TARGET_ARCH)


#-------------------------------------------------------------------------------
# Define common rules for single- and multiple-architecture builds.
#-------------------------------------------------------------------------------

# Add ranlib to standard rule for adding module to library.
#
# Since libtool does not support ar features like creating an archive if it
# does not exist, we want to use ar.  But ar cannot operate on fat archives
# after ranlib is used on them.  So we keep a separate copy of the library
# with the suffix .ar.  All ar operations on done on that, and then a copy
# is made, and ranlib is run on that.
#
# This is done whether or not ARCHS is defined because it produces a good
# library either way, and single-architecture builds might want to add a
# single-architecture module to a library in which other modules are
# multiple-architecture.
RANLIB	= ranlib
(%): %
	@echo
	@echo "#-- Adding $< to library $@. --"
	$(AR) $(ARFLAGS) $@.ar $<
	cp $@.ar $@
	$(RANLIB) $@

%: %.o
	@echo
	@echo "#-- Linking to $@. --"
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)


#-------------------------------------------------------------------------------


#-------------------------------------------------------------------------------
# Define rules for building for multiple architectures.
#-------------------------------------------------------------------------------
ifdef ARCHS


# Set flags for building for all architectures (used when linking).
TARGET_ARCH	= $(patsubst %, -arch %, $(ARCHS))

# Set compiler flags for building for specific architectures.
%.i386.o:	TARGET_ARCH = -arch i386 -msse3 $(CFLAGS_i386)
%.ppc.o:	TARGET_ARCH = -arch ppc -maltivec $(CFLAGS_ppc)
%.i386.s:	TARGET_ARCH = -arch i386 -msse3 $(CFLAGS_i386)
%.ppc.s:	TARGET_ARCH = -arch ppc -maltivec $(CFLAGS_ppc)

# Cancel old rules for compiling.
%.o:	%.c
%.o:	%.cpp
%.o:	%.f
%.o:	%.s
%:		%.c
%:		%.cpp
%:		%.f
%:		%.s

# Specify how to combine thin object files into a fat object file.
%.o:	$(patsubst %, \%.%.o, $(ARCHS))
	@echo
	@echo "#-- Merging thin files into fat file $@. --"
	lipo -create $(OUTPUT_OPTION) $^

%.i386.o:	%.c
	@echo
	@echo "-- Compiling $< to $@. --"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.ppc.o:	%.c
	@echo
	@echo "-- Compiling $< to $@. --"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.i386.s: %.c
	@echo
	@echo "#-- Compiling $< to assembly to $@. --"
	$(COMPILE.c) -S $<

%.ppc.s: %.c
	@echo
	@echo "#-- Compiling $< to assembly to $@. --"
	$(COMPILE.c) -S $<

%.i386.o: %.cpp
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

%.ppc.o: %.cpp
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

%.i386.o: %.f
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.f) $(OUTPUT_OPTION) $<

%.ppc.o: %.f
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.f) $(OUTPUT_OPTION) $<

%.i386.o: %.s
	@echo
	@echo "#-- Assembling $< to $@. --"
	$(COMPILE.S) $(OUTPUT_OPTION) $<

%.ppc.o: %.s
	@echo
	@echo "#-- Assembling $< to $@. --"
	$(COMPILE.S) $(OUTPUT_OPTION) $<


#-------------------------------------------------------------------------------


#-------------------------------------------------------------------------------
# Define rules for building for a single architecture (native unless
# TARGET_ARCH is defined).
#-------------------------------------------------------------------------------
else	# The following section is used if ARCHS is not defined.


%: %.c
	@echo
	@echo "#-- Compiling and linking $< to $@. --"
	$(LINK.c) $< $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)

%.o: %.c
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.s: %.c
	@echo
	@echo "#-- Compiling $< to assembly to $@. --"
	$(COMPILE.c) -S $<

%: %.cpp
	@echo
	@echo "#-- Compiling and linking $< to $@. --"
	$(LINK.cpp) $< $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)

%.o: %.cpp
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

%: %.f
	@echo
	@echo "#-- Compiling and linking $< to $@. --"
	$(LINK.f) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.o: %.f
	@echo
	@echo "#-- Compiling $< to $@. --"
	$(COMPILE.f) $(OUTPUT_OPTION) $<

%: %.s
	@echo
	@echo "#-- Assembling and linking $< to $@. --"
	$(LINK.S) $< $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)
		@# LINK.S and LINK.s use CC, which provides Apple behavior of
		@# preprocessing .s files.  So LINK.S is used here to pass the
		@# preprocessor flags (in CPPFLAGS).  On a non-Apple system, LINK.s
		@# can be used here to get non-Apple behavior.  To get non-Apple
		@# behavior on a non-Apple system, is necessary to use AS instead of
		@# CC, so the above command must be altered, by using AS directly or
		@# by changing LINK.s to use AS.

%.o: %.s
	@echo
	@echo "#-- Assembling $< to $@. --"
	$(COMPILE.S) $(OUTPUT_OPTION) $<
		@# Note that the dependency information produced for assembly files
		@# includes only files included with the C preprocessor's "#include"
		@# and not files included with the assembler's ".include".


endif	# End of conditionalization on ARCHS.
#-------------------------------------------------------------------------------
