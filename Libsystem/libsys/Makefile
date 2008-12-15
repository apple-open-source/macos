NAME = libc

NARCHS != echo $(RC_ARCHS) | wc -w
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
NJOBS != perl -e '$$n = `/usr/sbin/sysctl -n hw.ncpu`; printf "%d\n", $$n < 2 ? 2 : ($$n * 1.5)'
BSDMAKE = bsdmake -f Makefile
BSDMAKEJ = $(BSDMAKE) -j $(NJOBS)

# These variables are to guarantee that the left-hand side of an expression is
# always a variable
dynamic = dynamic

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
LIPOARGS$(F) != perl -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/$$_/$(F)/$(NAME)$(SUFFIX$(F)).a\"), qw($(RC_ARCHS))))'

.for A in $(RC_ARCHS)
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS
build-$(F):
.if $(NARCHS) == 1
	cp -p "$(OBJROOT)/$(RC_ARCHS)/$(F)/$(NAME)$(SUFFIX$(F)).a" "$(SYMROOT)"
.else
	lipo -create $(LIPOARGS$(F)) -output "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a"
.endif
	ranlib "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a"

.for A in $(RC_ARCHS)
build-$(A)-$(F): $(OBJROOT)/$(A)/libsystem.o
	mkdir -p '$(OBJROOT)/$(A)/$(F)'
	@set -x && \
	    cd '$(OBJROOT)/$(A)/$(F)' && \
	    if lipo -info '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' | grep -q '^Non-'; then \
		cp -p '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' .; \
	    else \
		arch=`lipo -detailed_info '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' | sed -E -n 's/.*architecture:? //p' | sort | grep '^$(A)' | head -n 1` && \
		lipo -thin $$arch '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' -output '$(NAME)-partial$(SUFFIX$(F)).a'; \
	    fi && \
	    ar x '$(NAME)-partial$(SUFFIX$(F)).a' && \
	    if lipo -info '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' | grep -q '^Non-'; then \
		cp -p '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' .; \
	    else \
		arch=`lipo -detailed_info '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' | sed -E -n 's/.*architecture:? //p' | sort | grep '^$(A)' | head -n 1` && \
		lipo -thin $$arch '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' -output 'libsyscall$(SUFFIX$(F)).a'; \
	    fi && \
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
	$(SDKROOT)/Developer/Makefiles/bin/version.pl Libsystem > $(.TARGET)

installhdrs:

.for F in $(FORMS)
BI-install-$(F): build-$(F)
	mkdir -p "$(DSTROOT)/usr/local/lib/system"
	@echo "===== Installing $(NAME)$(SUFFIX$(F)).a ====="
	install "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a" "$(DSTROOT)/usr/local/lib/system"
	ranlib "$(DSTROOT)/usr/local/lib/system/$(NAME)$(SUFFIX$(F)).a"
	chmod 0444 "$(DSTROOT)/usr/local/lib/system/$(NAME)$(SUFFIX$(F)).a"
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	rm -rf $(OBJROOT)/$(A)
.endfor # RC_ARCHS
