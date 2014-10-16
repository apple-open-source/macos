EXECUTABLE=keySizePref
# C++ source (with .cpp extension)
CPSOURCE= keySizePref.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

FRAMEWORKS= -framework CoreFoundation
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= 
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH)

#
# This assumes final load with cc, not ld
#
LIBS= -lstdc++
LIBPATH= 
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH)

first:	$(EXECUTABLE)

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(OFILES) $(FRAMEWORKS) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
