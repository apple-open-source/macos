
bin_PROGRAMS = makedepend

INCLUDES =  @X_CFLAGS@

makedepend_SOURCES = include.c \
                     main.c \
                     parse.c \
                     pr.c \
                     cppsetup.c \
                     ifparser.c \
                     ifparser.h \
                     def.h \
                     imakemdep.h

AM_CPPFLAGS = -DOBJSUFFIX=\".${OBJEXT}\" -DINCLUDEDIR=\"/usr/include\"
AM_CFLAGS = $(CWARNFLAGS)

EXTRA_DIST = autogen.sh makedepend.man

# Man page
appmandir = $(APP_MAN_DIR)

appman_PRE = makedepend.man
appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

CLEANFILES = $(appman_DATA)

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
MAN_SUBSTS += \
	-e 's|__cpp__|$(RAWCPP)|g'

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(makedepend_SOURCES)
endif LINT

### Test cases - mostly for bug fixes submitted with a test case so far
TESTS =

# Absolute path to srcdir & builddir top that test scripts can reference
TEST_BUILDDIR_PATH = `cd $(top_builddir) && pwd`
TEST_SRCDIR_PATH = `cd $(top_srcdir) && pwd`

# Path to makedepend command being tested that can be found from test subdirs
TESTS_ENVIRONMENT = MAKEDEPEND="$(TEST_BUILDDIR_PATH)/makedepend"

# Path to writable testdirectory in build directory
TESTS_ENVIRONMENT += test_builddir="$(TEST_BUILDDIR_PATH)/tests"

# Path to testcase files in source directory
TESTS_ENVIRONMENT += test_srcdir="$(TEST_SRCDIR_PATH)/tests"

# Test for https://bugs.freedesktop.org/show_bug.cgi?id=28045
TESTS += tests/28045/makedep.sh
EXTRA_DIST += tests/28045/makedep.sh tests/28045/foo.cpp
