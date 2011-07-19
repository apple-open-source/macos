bin_PROGRAMS = xcursorgen

xcursorgen_SOURCES = xcursorgen.c

AM_CFLAGS = $(CWARNFLAGS) $(XCURSORGEN_CFLAGS)

xcursorgen_LDADD = $(XCURSORGEN_LIBS)

appmandir = $(APP_MAN_DIR)

appman_PRE = xcursorgen.man
appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST = $(appman_PRE) autogen.sh
MAINTAINERCLEANFILES = ChangeLog INSTALL
CLEANFILES = $(appman_DATA)

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
