.ifndef DSTROOT
DSTROOT != x=`pwd`/DSTROOT && mkdir -p $$x && echo $$x
.endif
.ifndef OBJROOT
OBJROOT != x=`pwd`/OBJROOT && mkdir -p $$x && echo $$x
.endif
.ifndef SRCROOT
SRCROOT != dirname `pwd`
.endif
.ifndef SYMROOT
SYMROOT != x=`pwd`/SYMROOT && mkdir -p $$x && echo $$x
.endif
ARCH != arch
.ifndef RC_ARCHS
RC_ARCHS = $(ARCH)
RC_$(RC_ARCHS) = 1
.endif
NARCHS != echo $(RC_ARCHS) | wc -w
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(NEXT_ROOT)/usr/local/lib/system
.endif
NJOBS != perl -e '$$n = `/usr/sbin/sysctl -n hw.ncpu`; printf "%d\n", $$n < 2 ? 2 : ($$n * 1.5)'
BSDMAKE = bsdmake -f Makefile
BSDMAKEJ = $(BSDMAKE) -j $(NJOBS)

# This variables are to guarantee that the left-hand side of an expression is
# always a variable
dynamic = dynamic

# Remove the arch stuff, since we know better here.
LOCAL_CFLAGS != echo $(RC_CFLAGS) | sed 's/ *-arch [^ ][^ ]*//g'

FORMS := debug dynamic profile

all: build

build: build-debug build-dynamic build-profile

# These are the non B&I defaults
.ifndef RC_ProjectName
install: installhdrs install-all

.else # RC_ProjectName

.for F in $(FORMS)
install: BI-install-$(F)
.endfor # FORMS
.endif # RC_ProjectName

.for F in $(FORMS)
.if $(dynamic) == $(F)
SUFFIX$(F) =
.else
SUFFIX$(F) = _$(F)
.endif
LIPOARGS$(F) != perl -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/$$_/$(F)/libc$(SUFFIX$(F)).a\"), qw($(RC_ARCHS))))'

.for A in $(RC_ARCHS)
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS
build-$(F):
.if $(NARCHS) == 1
	cp -p "$(OBJROOT)/$(RC_ARCHS)/$(F)/libc$(SUFFIX$(F)).a" "$(SYMROOT)"
.else
	lipo -create $(LIPOARGS$(F)) -output "$(SYMROOT)/libc$(SUFFIX$(F)).a"
.endif
	ranlib "$(SYMROOT)/libc$(SUFFIX$(F)).a"

.for A in $(RC_ARCHS)
build-$(A)-$(F): $(OBJROOT)/$(A)/libsystem.o
	mkdir -p '$(OBJROOT)/$(A)/$(F)'
	@set -x && \
	    cd '$(OBJROOT)/$(A)/$(F)' && \
	    ditto -arch $(A) '$(LIBSYS)/libc-partial$(SUFFIX$(F)).a' . && \
	    ar x 'libc-partial$(SUFFIX$(F)).a' && \
	    ditto -arch $(A) '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' . && \
	    ar x 'libsyscall$(SUFFIX$(F)).a' && \
	    '$(SRCROOT)/libsys/build-libc.pl' '$(LIBSYS)' '$(OBJROOT)/$(A)/$(F)' && \
	    ln -s ../libsystem.o && \
	    $(BSDMAKEJ)
.endfor # RC_ARCHS
.endfor # FORMS

.for A in $(RC_ARCHS)
$(OBJROOT)/$(A)/libsystem.o: $(OBJROOT)/libsystem.c
	mkdir -p '$(OBJROOT)/$(A)'
	cc -c -o '$(.TARGET)' -arch $(A) '$(.ALLSRC)'
.endfor # RC_ARCHS

$(OBJROOT)/libsystem.c:
	/Developer/Makefiles/bin/version.pl Libsystem > $(.TARGET)

installhdrs:

.for F in $(FORMS)
BI-install-$(F): build-$(F)
	mkdir -p "$(DSTROOT)/usr/local/lib/system"
	@echo "Installing libc$(SUFFIX$(F)).a"
	install "$(SYMROOT)/libc$(SUFFIX$(F)).a" "$(DSTROOT)/usr/local/lib/system"
	ranlib "$(DSTROOT)/usr/local/lib/system/libc$(SUFFIX$(F)).a"
	chmod 0444 "$(DSTROOT)/usr/local/lib/system/libc$(SUFFIX$(F)).a"
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	rm -rf $(OBJROOT)/$(A)
.endfor # RC_ARCHS
