# name of executable to build
EXECUTABLE=dhTest
# C++ source (with .cpp extension)
CPSOURCE= dhTest.cpp
# C source (.c extension)
CSOURCE=

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
PROJ_CFLAGS=

#
# Optional link flags (using cc, not ld)
#
#PROJ_LDFLAGS= -lMallocDebug
PROJ_LDFLAGS= 

#
# Optional dependencies
#
PARAM_FILE_NAME= dhParams_512.der
PARAM_FILE= $(LOCAL_BUILD)/$(PARAM_FILE_NAME)
PROJ_DEPENDS= $(PARAM_FILE)

include ../Makefile.cdsa

$(PARAM_FILE):	$(PARAM_FILE_NAME)
	cp -p $(PARAM_FILE_NAME) $(LOCAL_BUILD)

