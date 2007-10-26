libmandir = $(LIB_MAN_DIR)

libman_PRE = Xcursor.man
libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

all-local: $(libman_DATA)

EXTRA_DIST = $(libman_PRE)
CLEANFILES = $(libman_DATA)

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@

.man.$(LIB_MAN_SUFFIX):
	-rm -f $@
	sed 's/__xorgversion__/"$(XORGRELSTRING)"/;  s/__LIB_MAN_SUFFIX__/$(LIB_MAN_SUFFIX)/; s%__prefix__%$(prefix)%g; s%__ICONDIR__%$(ICONDIR)%g' < $< > $@
