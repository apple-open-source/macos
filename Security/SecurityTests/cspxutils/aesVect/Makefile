# name of executable to build
EXECUTABLE= aesVect
# C++ source (with .cpp extension)
CPSOURCE= 
# C source (.c extension)
#CSOURCE= aesVect.c _rijndael-alg-ref.c _rijndaelApi.c enDecryptRef.c enDecryptTest.c \
#	enDecryptCsp.c rijndael.c
CSOURCE= aesVect.c enDecryptRef.c enDecryptTest.c enDecryptCsp.c rijndael.c

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
#PROJ_CFLAGS= -F/System/Library/PrivateFrameworks
PROJ_CFLAGS=

#
# Optional link flags (using cc, not ld)
#
PROJ_LDFLAGS=

#
# Other files to remove at 'make clean' time
#
OTHER_TO_CLEAN=

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.cdsa
