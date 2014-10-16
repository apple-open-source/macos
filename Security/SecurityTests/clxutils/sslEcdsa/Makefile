# name of executable to build
EXECUTABLE=sslEcdsa
# C++ source (with .cpp extension)
CPSOURCE= sslEcdsa.cpp
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
PROJ_FRAMEWORKS= -framework CoreFoundation
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
PROJ_LDFLAGS=

#
# Optional dependencies
#
KC_FILE_NAME= ecdsa.keychain
KC_FILE= $(LOCAL_BUILD)/$(KC_FILE_NAME)
PROJ_DEPENDS= $(KC_FILE)

HDR_DEPENDS=../clAppUtils/sslThreading.h

include ../Makefile.cdsa

$(KC_FILE):	$(KC_FILE_NAME)
	cp -p $(KC_FILE_NAME) $(KC_FILE)
