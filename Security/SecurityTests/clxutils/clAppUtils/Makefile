# name of executable to build
EXECUTABLE=libclutils.a
# C++ source (with .cpp extension)
CPSOURCE= BlobList.cpp CertBuilderApp.cpp CertParser.cpp \
	certVerify.cpp identPicker.cpp keyPicker.cpp printCertName.cpp \
	ringBufferIo.cpp sslRingBufferThreads.cpp sslServe.cpp sslClient.cpp \
	sslAppUtils.cpp timeStr.cpp tpUtils.cpp


# C source (.c extension)
CSOURCE= clutils.c ioSock.c

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
PROJ_CFLAGS= -fvisibility=hidden

#
# Optional link flags (using cc, not ld)
#
PROJ_LDFLAGS=

#
# Optional dependencies
#
PROJ_DEPENDS=

include ../Makefile.lib

# some crucial dependencies that must cause recompilation 
THREAD_OBJS= $(OBJROOT)/sslAppUtils.o $(OBJROOT)/sslClient.o $(OBJROOT)/sslServe.o

$(THREAD_OBJS): sslThreading.h

$(OBJROOT)/certVerify.o: certVerify.h

