# name of executable to build
EXECUTABLE=sigPerform
# C++ source (with .cpp extension)
CPSOURCE= 
# C source (.c extension)
CSOURCE= sigPerform.c 

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
PROJ_LIBS= 
#
# Optional lib search paths
#
# for bsafe....
PROJ_LIBPATH= -L$(LOCAL_BUILD)
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
PROJ_FRAMEWORKS= -F/System/Library/PrivateFrameworks -framework CoreFoundation

#
# project-specific includes, with leading -I
#
PROJ_INCLUDES= 

#
# Optional C flags (warnings, optimizations, etc.)
#
PROJ_CFLAGS= -F/System/Library/PrivateFrameworks

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
