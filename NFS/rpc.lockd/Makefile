#
# Makefile for rpc.lockd
#

Project = rpc.lockd

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers \
		-Wshadow -Wmissing-prototypes \
		-Wmissing-declarations -Wno-discard-qual
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 \
		 -F$(SDKROOT)/System/Library/PrivateFrameworks -I $(SRCROOT) -I $(OBJROOT) -fPIE
Extra_LD_Flags = -Wl,-pie -lutil -framework oncrpc -dead_strip

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8
LAUNCHDAEMONS_DIR = $(DSTROOT)/System/Library/LaunchDaemons

#
# Standard B&I targets
#
all: $(SYMROOT)/rpc.lockd

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/rpc.lockd $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/rpc.lockd.8 $(MAN_DIR)
	install -d -o root -g wheel -m 755 $(LAUNCHDAEMONS_DIR)
	install -c -o root -g wheel -m 644 $(SRCROOT)/com.apple.lockd.plist $(DSTROOT)/System/Library/LaunchDaemons

clean::
	-rm -f *.o rpc.lockd $(MIG_GEN_FILES)
#
# Build
#

MIG_DEF_FILES = $(SRCROOT)/lockd_mach.defs
MIG_GEN_CFILES = lockd_machServer.c lockd_machUser.c
MIG_GEN_HFILES = lockd_mach.h lockd_machServer.h
MIG_GEN_FILES = $(MIG_GEN_CFILES) $(MIG_GEN_HFILES)
MIG_OUT_FILES = $(MIG_GEN_FILES:%=$(OBJROOT)/%)
CFILES = kern.c lock_proc.c lockd.c lockd_lock.c nlm_prot_svc.c \
         nlm_prot_xdr.c sm_inter_xdr.c
HFILES = lockd.h lockd_lock.h
OFILES = $(CFILES:.c=.o) lockd_machServer.o

#$(CFILES:%=$(SRCROOT)/%): $(HFILES:%=$(SRCROOT)/%) $(MIG_OUT_FILES)

$(OFILES:%=$(OBJROOT)/%): $(HFILES:%=$(SRCROOT)/%) $(MIG_GEN_HFILES:%=$(OBJROOT)/%)
$(OFILES): $(HFILES) $(MIG_GEN_HFILES)

#$(MIG_GEN_CFILES:%.c=$(OBJROOT)/%.o): $(MIG_GEN_CFILES:%.c=$(OBJROOT)/%.c)
#	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(MIG_OUT_FILES): $(MIG_DEF_FILES)
	mig \
		-user    $(OBJROOT)/lockd_machUser.c \
		-header  $(OBJROOT)/lockd_mach.h \
		-server  $(OBJROOT)/lockd_machServer.c \
		-sheader $(OBJROOT)/lockd_machServer.h \
		$(MIG_DEF_FILES)

$(SYMROOT)/rpc.lockd: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

