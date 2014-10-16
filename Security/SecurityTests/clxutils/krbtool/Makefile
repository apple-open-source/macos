EXECUTABLE=krbtool
# C++ source (with .cpp extension)
CPSOURCE= krbtool.cpp identPicker.cpp asnUtils.cpp
# C source (.c extension)
CSOURCE=
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

CC=c++

FRAMEWORKS= -framework Security -framework CoreFoundation -framework Kerberos
FRAME_SEARCH= -F$(LOCAL_BUILD)
FINCLUDES= 
PINCLUDES= 
OTHER_CFLAGS=
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
###WFLAGS= -Wno-four-char-constants -Wmost -Werror
WFLAGS= -Wno-four-char-constants -Wmost -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(OTHER_CFLAGS) $(FRAME_SEARCH)

#
# This assumes final load with cc, not ld
#
LIBS= -lstdc++
LIBPATH= 
OTHER_LDFLAGS=
LDFLAGS= $(LIBS) $(LIBPATH) $(OTHER_LDFLAGS) $(FRAME_SEARCH) -lsecurity_cdsa_utils

first:	$(EXECUTABLE)

install:

64bit:
	make "OTHER_CFLAGS=-arch ppc64" "OTHER_LDFLAGS=-arch ppc64" 
	
64bitFat:
	make "OTHER_CFLAGS=-arch ppc64" "OTHER_LDFLAGS=-arch ppc64" 

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
