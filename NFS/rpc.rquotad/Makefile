#
# Makefile for rpc.rquotad
#

Project = rpc.rquotad

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -F$(SDKROOT)/System/Library/PrivateFrameworks -fPIE
Extra_LD_Flags = -Wl,-pie -lutil -framework oncrpc -dead_strip

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/libexec
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: $(SYMROOT)/rpc.rquotad

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/rpc.rquotad $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/rpc.rquotad.8 $(MAN_DIR)

clean::
	-rm -f *.o rpc.rquotad

#
# Build
#
CFILES = rquotad.c rquota_xdr.c
HFILES = rquota.h
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/rpc.rquotad: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

