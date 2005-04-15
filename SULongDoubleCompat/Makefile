#
# Makefile for SULongDoubleCompat project
#
# This project installs a set of archives that contains 128-bit long double
# support (the archives are built by Libc in Tiger).
#
# The default rule should be run on Tiger to create a tarball of the archives.
# Then, in a build train, the tarball in unpack back into /usr/local/lib/system.
#

SOURCES = Makefile $(TARBALL)

ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
else
LIBSYS = $(NEXT_ROOT)$(USRLOCALLIBSYSTEM)
endif

USRLOCALLIBSYSTEM = /usr/local/lib/system
LIBNAME = libldbl128
SUFFIXES = .a _debug.a _profile.a
LIBGCC = libgcc-ppc.a
BINARIES = $(foreach S,$(SUFFIXES),$(LIBNAME)$(S)) $(LIBGCC)
TARBALL = SULongDoubleCompat.tgz

default:
	rm -f $(TARBALL)
	tar czf $(TARBALL) -C "$(LIBSYS)" $(BINARIES)

install:
	install -d "$(DSTROOT)$(USRLOCALLIBSYSTEM)"
	tar xzf $(TARBALL) -C "$(DSTROOT)$(USRLOCALLIBSYSTEM)"

installhdrs:

installsrc:
	mkdir -p "$(SRCROOT)"
	pax -rw $(SOURCES) "$(SRCROOT)"

clean:
