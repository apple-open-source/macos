# name of executable to build
EXECUTABLE=p12
# C++ source (with .cpp extension)
CPSOURCE= p12.cpp p12Crypto.cpp p12Decode.cpp p12pbe.cpp pkcs12Parsed.cpp \
	p12Encode.cpp p12ImportExport.cpp p12Parse.cpp p12GetPassKey.cpp SecNssCoder.cpp
# C source (.c extension)
CSOURCE=

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
PROJ_FRAMEWORKS= -framework securityd_client$(VARIANT_SUFFIX)


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
PROJ_LDFLAGS=-lcrypto

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.cdsa
