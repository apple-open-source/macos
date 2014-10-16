EXECUTABLE=genErrorStrings
# C++ source (with .cpp extension)
CPSOURCE= genErrorStrings.cpp
# C source (.c extension)
CSOURCE=fileIo.c
OFILES = $(CSOURCE:%.c=%.o) $(CPSOURCE:%.cpp=%.o)

FRAMEWORKS= 
FRAME_SEARCH=
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
	$(CC) -o $(EXECUTABLE) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(EXECUTABLE)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
