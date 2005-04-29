DIR = tests
include ../MakePaths.dir

CFLAGS  = $(RC_CFLAGS) $(OPTIM) $(MORECPP) -arch i386 -g -Wmost -Werror \
          -fno-builtin -DSAIO_INTERNAL_USER -static \
          -fomit-frame-pointer -mpreferred-stack-boundary=2 \
          -fno-align-functions
TESTS= satest
LIBSA= $(SYMROOT)/libsa.a
LIBSADIR = ../libsa
INC = -I. -I.. -I$(SYMROOT) -I$(LIBSADIR)

OBJROOT=../../obj/i386
SYMROOT=../../sym/i386
DSTROOT=../../dst/i386
SRCROOT=/tmp
VPATH=$(SYMROOT):$(OBJROOT)

DIRS_NEEDED = $(OBJROOT) $(SYMROOT)

all: $(TESTS)

satest: satest.o $(LIBSA)
	cc $(CFLAGS) -o satest $(filter %.o,$^) -L$(SYMROOT) -lsa -L/usr/local/lib/system -lc_static -lgcc_static

include ../MakeInc.dir
