EXECUTABLE=hashTimeSA
# C++ source (with .cpp extension)
CPSOURCE= hashTimeSA.cpp 
# C source (.c extension)
CSOURCE= SHA1_priv.c MD5.c SHA1.c
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

FRAMEWORKS= -framework CoreFoundation
FRAME_SEARCH= 
FINCLUDES= 
PINCLUDES= -I$(LOCAL_BUILD)/include
CINCLUDES= $(FINCLUDES) $(PINCLUDES)
WFLAGS= -Wno-four-char-constants -Wno-deprecated-declarations
CFLAGS= -g $(CINCLUDES) $(WFLAGS) $(FRAME_SEARCH) -Os

#
# This assumes final load with cc, not ld
#
###LIBS= -lstdc++ -lcommonCrypto
LIBS= -lstdc++
LIBPATH= -L$(LOCAL_BUILD) -L/usr/local/lib/system
LDFLAGS= $(LIBS) $(LIBPATH) $(FRAME_SEARCH)

first:	$(EXECUTABLE)

$(EXECUTABLE):	$(OFILES)
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

installhdrs:

install:

%.o: %.c
	$(CC) $(CFLAGS) -c -o $*.o $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $*.o $<
