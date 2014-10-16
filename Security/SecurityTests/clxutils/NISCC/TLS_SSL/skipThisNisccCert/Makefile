EXECUTABLE=skipThisNisccCert
# C++ source (with .cpp extension)
CPSOURCE= skipThisNisccCert.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

#
# Executable in build folder
#
BUILT_TARGET= $(LOCAL_BUILD)/$(EXECUTABLE)

CC= /usr/bin/cc

FRAMEWORKS=
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= 
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH) 

#
# This assumes final load with cc, not ld
#
LIBS= -lstdc++ -lCdsaUtils -lnssasn1
LIBPATH= -L$(LOCAL_BUILD)
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH)

first:	$(BUILT_TARGET)

$(BUILT_TARGET):	$(OFILES)
	cc -o $(BUILT_TARGET) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(BUILT_TARGET)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
