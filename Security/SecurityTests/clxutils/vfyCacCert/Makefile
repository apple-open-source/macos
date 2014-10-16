EXECUTABLE=vfyCacCert
# C++ source (with .cpp extension)
CPSOURCE= vfyCacCert.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

CC=c++

FRAMEWORKS= -framework Security -framework CoreFoundation -framework security_cdsa_utils
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= 
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH)

#
# This assumes final load with cc, not ld
#
LIBS=
LIBPATH= -L$(LOCAL_BUILD)
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH)

first:	$(EXECUTABLE)

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
