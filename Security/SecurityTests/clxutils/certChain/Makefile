EXECUTABLE=certChain
# C++ source (with .cpp extension)
CPSOURCE= certChain.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

CC=c++

FRAMEWORKS= -framework Security -framework CoreFoundation
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= 
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
DEBUG_CFLAGS?=
CMDLINE_LDFLAGS?=

CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH) $(DEBUG_CFLAGS)

#
# This assumes final load with cc, not ld
#
LIBS= 
LIBPATH=
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH) $(CMDLINE_LDFLAGS) -lsecurity_cdsa_utils 

first:	$(EXECUTABLE)

install:

installhdrs:

64bit:
	make "DEBUG_CFLAGS=-arch ppc64" "CMDLINE_LDFLAGS=-arch ppc64" 
	
64bitFat:
	make "DEBUG_CFLAGS=-arch ppc64" "CMDLINE_LDFLAGS=-arch ppc64" 
	

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $*.o $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $*.o $<
