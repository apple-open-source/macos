EXECUTABLE=sha2Vectors
# C++ source (with .cpp extension)
CPSOURCE= sha2Vectors.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

VARIANT_SUFFIX=

FRAMEWORKS=
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= -I$(LOCAL_BUILD)/include
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH)

#
# This assumes final load with cc, not ld
#
#LIBS= -lstdc++ -lcommonCrypto$(VARIANT_SUFFIX)
LIBS= -lstdc++

# this is temporary 
LIBPATH= -L$(LOCAL_BUILD) -L/usr/local/lib/system
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH)

first:	$(EXECUTABLE)

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

debug:
	make "VARIANT_SUFFIX=_debug"

clean:
	rm -f *.o $(EXECUTABLE)

installhdrs:

install:

%.o: %.c
	$(CC) $(CFLAGS) -c -o $*.o $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $*.o $<
