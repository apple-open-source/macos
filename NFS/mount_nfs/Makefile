#
# Makefile for mount_nfs
#

Project = mount_nfs

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow \
		-Wno-deprecated-declarations
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -isysroot $(SDKROOT) \
		 -fPIE
Extra_LD_Flags = -Wl,-pie -lutil -dead_strip

DST_DIR = $(DSTROOT)/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: $(SYMROOT)/mount_nfs

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 4555 -s $(SYMROOT)/mount_nfs $(DST_DIR)
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share/man
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/mount_nfs.8 $(MAN_DIR)

clean::
	-rm -f *.o mount_nfs

#
# Build
#
CFILES = mount_nfs.c
OFILES = $(CFILES:.c=.o)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/mount_nfs: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

