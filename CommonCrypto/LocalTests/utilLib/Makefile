# name of executable to build
EXECUTABLE=libcsputils.a
# C++ source (with .cpp extension)
CPSOURCE= commonCpp.cpp nssAppUtils.cpp ssleayUtils.cpp
# C source (.c extension)
CSOURCE= common.c cspwrap.c fileIo.c bsafeUtils.c t_stdlib.c rijndael-alg-ref.c \
	rijndaelApi.c cputime.c

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
PROJ_FRAMEWORKS=

#
# project-specific includes, with leading -I
#
PROJ_INCLUDES= 

#
# Optional C flags (warnings, optimizations, etc.)
#
#PROJ_CFLAGS= -Os
PROJ_CFLAGS= 

#
# Optional link flags (using cc, not ld)
#
PROJ_LDFLAGS=

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.lib

# Special case for this object file...normally we ignore header dependencies, but
# this header is auto generated on a regular basis. 
$(OBJROOT)/commonCpp.o: commonCpp.cpp cssmErrorStrings.h
	$(CC) $(ALL_CFLAGS) -c -o $(OBJROOT)/commonCpp.o commonCpp.cpp
