# name of executable to build
EXECUTABLE=ccHmacClone
# C++ source (with .cpp extension)
CPSOURCE= ccHmacClone.cpp
# C source (.c extension)
CSOURCE= 

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
#PROJ_LIBS= -lcommonCrypto -lcrypto 
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
PROJ_INCLUDES= -I$(LOCAL_BUILD)/include

#
# Optional C flags (warnings, optimizations, etc.)
#
PROJ_CFLAGS= -F/System/Library/PrivateFrameworks

#
# Optional link flags (using cc, not ld)
#
#PROJ_LDFLAGS= -lMallocDebug -lBSafe
#
# this is for linking against a custom libSystem...this is where it goes.
# You also need to do a setenv DYLD_LIBRARY_PATH /tmp/System/Debug to run.
#PROJ_LDFLAGS= -L/tmp/System/Debug
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
