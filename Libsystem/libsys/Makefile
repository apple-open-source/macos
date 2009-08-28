NAME = libc

.include <CoreOS/Standard/Commands.mk>
.include <CoreOS/Standard/Variables.mk>

NARCHS != $(ECHO) $(RC_ARCHS) | $(WC) -w
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
NJOBS != $(PERL) -e '$$n = `$(SYSCTL) -n hw.ncpu`; printf "%d\n", $$n < 2 ? 2 : ($$n * 1.5)'
BSDMAKEJ = $(BSDMAKE) -f Makefile -j $(NJOBS)

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
LIPOARGS$(F) != $(PERL) -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/$$_/$(F)/$(NAME)$(SUFFIX$(F)).a\"), qw($(RC_ARCHS))))'

.for A in $(RC_ARCHS)
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS
build-$(F):
.if $(NARCHS) == 1
	$(CP) "$(OBJROOT)/$(RC_ARCHS)/$(F)/$(NAME)$(SUFFIX$(F)).a" "$(SYMROOT)"
.else
	$(LIPO) -create $(LIPOARGS$(F)) -output "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a"
.endif
	$(RANLIB) "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a"

.for A in $(RC_ARCHS)
build-$(A)-$(F): $(OBJROOT)/$(A)/libsystem.o
	$(MKDIR) '$(OBJROOT)/$(A)/$(F)'
	@set -x && \
	    cd '$(OBJROOT)/$(A)/$(F)' && \
	    if $(LIPO) -info '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' | $(GREP) -q '^Non-'; then \
		$(CP) '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' .; \
	    else \
		arch=`$(LIPO) -detailed_info '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' | $(SED) -E -n 's/.*architecture:? //p' | $(SORT) | $(GREP) '^$(A)' | $(HEAD) -n 1` && \
		$(LIPO) -thin $$arch '$(LIBSYS)/$(NAME)-partial$(SUFFIX$(F)).a' -output '$(NAME)-partial$(SUFFIX$(F)).a'; \
	    fi && \
	    $(AR) x '$(NAME)-partial$(SUFFIX$(F)).a' && \
	    if $(LIPO) -info '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' | $(GREP) -q '^Non-'; then \
		$(CP) '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' .; \
	    else \
		arch=`$(LIPO) -detailed_info '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' | $(SED) -E -n 's/.*architecture:? //p' | $(SORT) | $(GREP) '^$(A)' | $(HEAD) -n 1` && \
		$(LIPO) -thin $$arch '$(LIBSYS)/libsyscall$(SUFFIX$(F)).a' -output 'libsyscall$(SUFFIX$(F)).a'; \
	    fi && \
	    $(AR) x 'libsyscall$(SUFFIX$(F)).a' && \
	    NM=$(NM) '$(SRCROOT)/libsys/build-libc.pl' '$(LIBSYS)' '$(OBJROOT)/$(A)/$(F)' && \
	    $(LN) -s ../libsystem.o && \
	    $(BSDMAKEJ)
.endfor # RC_ARCHS
.endfor # FORMS

.for A in $(RC_ARCHS)
$(OBJROOT)/$(A)/libsystem.o: $(OBJROOT)/libsystem.c
	$(MKDIR) '$(OBJROOT)/$(A)'
	$(CC) -c -o '$(.TARGET)' -arch $(A) '$(.ALLSRC)'
.endfor # RC_ARCHS

$(OBJROOT)/libsystem.c:
	$(SDKROOT)/Developer/Makefiles/bin/version.pl Libsystem > $(.TARGET)

installhdrs:

.for F in $(FORMS)
BI-install-$(F): build-$(F)
	$(MKDIR) "$(DSTROOT)/usr/local/lib/system"
	@$(ECHO) "===== Installing $(NAME)$(SUFFIX$(F)).a ====="
	$(INSTALL) "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).a" "$(DSTROOT)/usr/local/lib/system"
	$(RANLIB) "$(DSTROOT)/usr/local/lib/system/$(NAME)$(SUFFIX$(F)).a"
	$(CHMOD) 0444 "$(DSTROOT)/usr/local/lib/system/$(NAME)$(SUFFIX$(F)).a"
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	$(RMDIR) $(OBJROOT)/$(A)
.endfor # RC_ARCHS
