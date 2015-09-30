#
# Makefile for ncctl
#

Project = ncctl

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -fPIE -F$(SDKROOT)/System/Library/PrivateFrameworks
Extra_LD_Flags = -Wl,-pie -dead_strip -framework Heimdal

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/bin
MAN_DIR = $(DSTROOT)/usr/share/man/man1

#
# Standard B&I targets
#
all: $(SYMROOT)/$(Project)

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/$(Project) $(DST_DIR)
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/ncdestroy
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/ncinit
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/nclist
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/$(Project).1 $(MAN_DIR)
	ln -sf $(Project).1 $(MAN_DIR)/ncdestroy.1
	ln -sf $(Project).1 $(MAN_DIR)/ncinit.1
	ln -sf $(Project).1 $(MAN_DIR)/nclist.1
clean::
	-rm -rf *.o $(Project) ncestroy ncinit nclist $(Project).DSYM

#
# Build
#
CFILES = $(Project).c
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/$(Project): $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

