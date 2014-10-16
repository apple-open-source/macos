# name of executable to build
EXECUTABLE=ocspTool
# C++ source (with .cpp extension)
CPSOURCE= ocspTool.cpp ocspUtils.cpp ocspNetwork.cpp ocspRequest.cpp findOcspUrl.cpp
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
PROJ_FRAMEWORKS= -framework CoreFoundation -framework CoreServices
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
PROJ_LDFLAGS=-lsecurity_ocspd

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.cdsa
