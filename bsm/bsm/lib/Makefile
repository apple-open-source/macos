## Probably only works with GNU make

ifndef CFLAGS
CFLAGS = -g -Wall -pipe -fno-common
endif

GCC=gcc
LIBCFLAGS = $(CFLAGS) -I. -dynamic
LIB_NAME=libbsm.dylib
LIBCURVERS=1.0
LIBCOMPATVERS=1.0

LIB_HDRS=libbsm.h audit_uevents.h
LIB_HDRS_DIR=/usr/include/bsm

all: $(LIB_NAME)

$(LIB_NAME): 
	$(GCC) $(LIBCFLAGS) -c bsm_audit.c 
	$(GCC) $(LIBCFLAGS) -c bsm_io.c 
	$(GCC) $(LIBCFLAGS) -c bsm_class.c 
	$(GCC) $(LIBCFLAGS) -c bsm_control.c 
	$(GCC) $(LIBCFLAGS) -c bsm_event.c 
	$(GCC) $(LIBCFLAGS) -c bsm_flags.c 
	$(GCC) $(LIBCFLAGS) -c bsm_mask.c 
	$(GCC) $(LIBCFLAGS) -c bsm_token.c 
	$(GCC) $(LIBCFLAGS) -c bsm_user.c 
	$(GCC) $(LIBCFLAGS) -c bsm_notify.c 
	$(GCC) $(LIBCFLAGS) -c lib_wrappers.c 
	$(GCC) -dynamiclib $(CFLAGS) -install_name /usr/lib/$(LIB_NAME) -compatibility_version $(LIBCOMPATVERS) -current_version $(LIBCURVERS) *.o -o $(LIB_NAME)

install: $(LIB_NAME)
	install -d /usr/lib
	install $(LIB_NAME) /usr/lib
	/usr/bin/strip -S /usr/lib/$(LIB_NAME)
	install -d $(LIB_HDRS_DIR)
	install -m 0444 $(LIB_HDRS) $(LIB_HDRS_DIR)

clean:
	rm -f *.o $(LIB_NAME)
