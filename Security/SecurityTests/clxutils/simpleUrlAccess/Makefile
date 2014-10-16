EXECUTABLE=simpleUrlAccess
SOURCE= simpleUrlAccess.c
OFILES = $(SOURCE:%.c=%.o)

LOCAL_BUILD= $(shell echo $(LOCAL_BUILD_DIR))

ifeq "" "$(LOCAL_BUILD)"
	LOCAL_BUILD = .
endif


#
# This assumes final load with cc, not ld
#
LIBS= 
LIBPATH=
LDFLAGS= $(LIBS) $(LIBPATH)

CC= /usr/bin/cc

FRAMEWORKS= -framework Carbon
FINCLUDES= 
PINCLUDES=
CINCLUDES= $(FINCLUDES) $(PINCLUDES)

#
# -Werror impossible when including Carbon.h per Radar 2731547
#
WFLAGS= -Wno-four-char-constants -Wmost -Wno-deprecated-declarations
#WFLAGS= -Wno-four-char-constants -Wmost -Werror
CFLAGS= -g $(CINCLUDES) $(WFLAGS) -D__STDC__

BUILT_TARGET= $(LOCAL_BUILD)/$(EXECUTABLE)

first:	$(BUILT_TARGET)

$(BUILT_TARGET):	$(OFILES)
	cc -o $(BUILT_TARGET) $(FRAMEWORKS) $(OFILES) $(LDFLAGS)

clean:
	rm -f *.o $(BUILT_TARGET)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
