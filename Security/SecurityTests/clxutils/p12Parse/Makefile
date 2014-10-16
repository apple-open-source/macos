# name of executable to build
EXECUTABLE=p12Parse
# C++ source (with .cpp extension)
CPSOURCE= main.cpp p12Crypto.cpp pkcs12Parsed.cpp \
	p12Parse.cpp pkcs12Utils.cpp
# C source (.c extension)
CSOURCE=

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
#
# Note: normally security_pkcs12 is not usable outside of Security.framework since
# it uses private routines in libsecurity_keychain. However we're just pulling in the
# ASN1 templates from pkcs12Templates and SecNssCoder...safe...for now.
#
PROJ_FRAMEWORKS= # -framework security_asn1 -framework security_pkcs12 


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
