# name of executable to build
EXECUTABLE=p12Reencode
# C++ source (with .cpp extension)
CPSOURCE= p12Reencode.cpp
# C source (.c extension)
CSOURCE=

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
PROJ_FRAMEWORKS= -framework security_pkcs12$(VARIANT_SUFFIX) -framework security_apple_csp$(VARIANT_SUFFIX) -framework security_keychain$(VARIANT_SUFFIX)

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
PROJ_DEPENDS=

include ../Makefile.cdsa
