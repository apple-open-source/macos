
DIR = libsa
include ../MakePaths.dir

UTILDIR = ../util
INSTALL_MI_DIR = $(DSTROOT)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/machdep/machine

DEBUG = -O
CFLAGS	= $(DEBUG) $(MORECPP) -arch i386 -g -Wmost -Wno-precomp -munaligned-text
DEFINES=
CONFIG = hd
INC = -I. -I$(SYMROOT) -I$(UTILDIR)
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
AS = as
LD = ld
# LIBS= -lc_static
LIBS=

VPATH = $(OBJROOT):$(SYMROOT)

INSTALLED_MI_HFILES = kernBootStruct.h

clean::
	@echo "Nothing to clean (yet)"

all:
	@echo "Nothing to build (yet)"

$(INSTALL_MI_DIR):
	$(MKDIRS) $@

installhdrs:: $(INSTALL_MI_DIR)
	cp $(INSTALLED_MI_HFILES) $(INSTALL_MI_DIR)

include ../MakeInc.dir

# dependencies
-include $(OBJROOT)/Makedep
