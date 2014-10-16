EXECUTABLE=dotMacTool
# C++ source (with .cpp extension)
CPSOURCE= dotMacTool.cpp keyPicker.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

CC=c++
VARIANT_SUFFIX=

FRAMEWORKS=-framework Security$(VARIANT_SUFFIX) -framework CoreFoundation
FRAME_SEARCH= -F$(LOCAL_BUILD) -F/usr/local/SecurityPieces/Frameworks
FINCLUDES= 
PINCLUDES= 
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
###WFLAGS= -Wno-four-char-constants -Wall -Werror
WFLAGS= -Wno-four-char-constants -Wall -Wno-deprecated-declarations
DEBUG_CFLAGS?=
CMDLINE_LDFLAGS?=
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH) $(DEBUG_CFLAGS)

#
# This assumes final load with cc, not ld
#
LIBS= -lstdc++
LIBPATH= -L$(LOCAL_BUILD)
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH) $(CMDLINE_LDFLAGS) -lsecurity_cdsa_utils -lsecurity_utilities

#
# Executable in build folder
#
BUILT_TARGET= $(LOCAL_BUILD)/$(EXECUTABLE)

first:	$(BUILT_TARGET)

install:

debug:
	make "VARIANT_SUFFIX=,_debug"

64bit:
	make "DEBUG_CFLAGS=-arch ppc64" "CMDLINE_LDFLAGS=-arch ppc64" 
	
64bitFat:
	make "DEBUG_CFLAGS=-arch ppc -arch ppc64" "CMDLINE_LDFLAGS=-arch ppc -arch ppc64" 

$(BUILT_TARGET):	$(OFILES)
	$(CC) -o $(BUILT_TARGET) $(OFILES) $(FRAMEWORKS) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $*.o $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $*.o $<
