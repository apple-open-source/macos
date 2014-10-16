# name of executable to build
EXECUTABLE=hashTime
# C++ source (with .cpp extension)
CPSOURCE=hashTime.cpp
# C source (.c extension)
CSOURCE= MD5.c SHA1_priv.c SHA1.c

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
PROJ_LIBS= -lcrypto
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
# temp for old cputime
PROJ_FRAMEWORKS=-framework IOKit

#
# project-specific includes, with leading -I
#
PROJ_INCLUDES=-I$(LOCAL_BUILD)/include

#
# Optional C flags (warnings, optimizations, etc.)
#
PROJ_CFLAGS=-Os

#
# Optional link flags (using cc, not ld)
#
#PROJ_LDFLAGS= -lMallocDebug
PROJ_LDFLAGS=

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.cdsa
