bin_PROGRAMS = xcursorgen

xcursorgen_SOURCES = xcursorgen.c

xcursorgen_CFLAGS = @XCURSORGEN_CFLAGS@

xcursorgen_LDADD = @XCURSORGEN_LIBS@

appmandir = $(APP_MAN_DIR)

appman_PRE = xcursorgen.man
appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST = $(appman_PRE)
CLEANFILES = $(appman_DATA)

MAN_SUBSTS = -e 's|__vendorversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' 

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@
