NAME0 = libSystem
NAME = $(NAME0).$(VersionLetter)

.include <CoreOS/Standard/Commands.mk>
.include <CoreOS/Standard/Variables.mk>

# for now, use the default compiler
MYCC := $(CC)
.if $(RC_TARGET_CONFIG) == iPhone
MYCCLIBS = -lgcc
.endif
RTLIBS =
NARCHS != $(ECHO) $(RC_ARCHS) | $(WC) -w
SLFS_F_PH = $(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
CODESIGN != xcrun -find codesign
.ifdef SDKROOT
SDKROOTCFLAGS = -isysroot '$(SDKROOT)'
SDKROOTLDFLAGS = -Wl,-syslibroot,'$(SDKROOT)'
.endif
ORDERFILES = -Wl,-order_file,$(SRCROOT)/SystemInit.order -Wl,-order_file,$(PLATFORM_ORDER_FILE)

.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
.ifdef ALTUSRLIBSYSTEM
LSYS = $(ALTUSRLIBSYSTEM)
.else
LSYS = $(SDKROOT)/usr/lib/system
.endif

ACTUALLIBS = $(SYMROOT)/actuallibs
ALLLIBS = $(SYMROOT)/alllibs
FROMUSRLIBSYSTEM = $(SYMROOT)/fromusrlibsystem
FROMUSRLOCALLIBSYSTEM = $(SYMROOT)/fromusrlocallibsystem
INUSRLIBSYSTEM = $(SYMROOT)/inusrlibsystem
INUSRLOCALLIBSYSTEM = $(SYMROOT)/inusrlocallibsystem
MISSINGLIBS = $(SYMROOT)/missinglibs
OPTIONALLIBS = $(SRCROOT)/optionallibs
POSSIBLEUSRLOCALLIBSYSTEM = $(SYMROOT)/possibleusrlocallibsystem
REQUIREDLIBS = $(SRCROOT)/requiredlibs

$(MISSINGLIBS):
	cat $(REQUIREDLIBS) $(OPTIONALLIBS) | sort > $(ALLLIBS)
	cd $(LSYS) && ls lib*.dylib | sed -E -e 's/^lib//' -e 's/\..*$$//' -e 's/_(debug|profile|static)$$//' | sort -u > $(INUSRLIBSYSTEM)
	cd $(LIBSYS) && ls lib*.a | sed -E -e 's/^lib//' -e 's/\..*$$//' -e 's/_(debug|profile|static)$$//' | sort -u > $(INUSRLOCALLIBSYSTEM)
	comm -12 $(ALLLIBS) $(INUSRLIBSYSTEM) > $(FROMUSRLIBSYSTEM)
	comm -12 $(ALLLIBS) $(INUSRLOCALLIBSYSTEM) > $(POSSIBLEUSRLOCALLIBSYSTEM)
	comm -13 $(FROMUSRLIBSYSTEM) $(POSSIBLEUSRLOCALLIBSYSTEM) > $(FROMUSRLOCALLIBSYSTEM)
	cat $(FROMUSRLIBSYSTEM) $(FROMUSRLOCALLIBSYSTEM) | sort > $(ACTUALLIBS)
	comm -23 $(REQUIREDLIBS) $(ACTUALLIBS) > $(MISSINGLIBS)
	@if [ -s $(MISSINGLIBS) ]; then \
	    echo '*** missing required libs ***' && \
	    cat $(MISSINGLIBS) && \
	    exit 1; \
	fi ;

# These variables are to guarantee that the left-hand side of an expression is
# always a variable
dynamic = dynamic

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

.for A in $(RC_ARCHS)
OBJS-$(A) = $(OBJROOT)/$(A)/SystemMath.o $(OBJROOT)/$(A)/CompatibilityHacks.o $(OBJROOT)/$(A)/System_vers.o $(OBJROOT)/$(A)/init.o
.endfor # RC_ARCHS

.for F in $(FORMS)
.if $(dynamic) == $(F)
SUFFIX$(F) =
.else
SUFFIX$(F) = _$(F)
.endif
LIPOARGS$(F) != $(PERL) -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/$$_/$(F)/$(NAME)$(SUFFIX$(F)).dylib\"), qw($(RC_ARCHS))))'

.for A in $(RC_ARCHS)
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS
build-$(F):
.if $(NARCHS) == 1
	$(CP) "$(OBJROOT)/$(RC_ARCHS)/$(F)/$(NAME)$(SUFFIX$(F)).dylib" "$(SYMROOT)"
.else
	$(LIPO) -create $(LIPOARGS$(F)) -output "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib"
.endif
	$(DSYMUTIL) "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib"

.for A in $(RC_ARCHS)
# Hardcode libc.a for now.  This will have to be changed when libc becomes
# its own dylib.
LINKDYLIB-$(F)-$(A) = $(MYCC) -dynamiclib -arch $(A) -pipe $(SDKROOTLDFLAGS) \
	-o '$(OBJROOT)/$(A)/$(F)/$(NAME)$(SUFFIX$(F)).dylib' \
	-compatibility_version 1 -current_version $(Version) \
	-install_name /usr/lib/$(NAME)$(SUFFIX$(F)).dylib \
	-nodefaultlibs -Wl,-search_paths_first \
	$(ORDERFILES) $(SKDROOTLDFLAGS) $(OBJS-$(A)) \
	-L$(LSYS) -L$(LIBSYS) $(LIBMATHCOMMON$(F)) \
	`sed 's/.*/-Wl,-reexport-l&/' $(FROMUSRLIBSYSTEM)` \
	`sed -e '/^c$$/d' -e 's|.*|-Wl,-force_load,$(LIBSYS)/lib&$(SUFFIX$(F)).a|' $(FROMUSRLOCALLIBSYSTEM)`

build-$(A)-$(F): $(OBJROOT)/$(A)/$(F) $(OBJS-$(A)) $(MISSINGLIBS)
	@$(ECHO) '========================================='
	@$(ECHO) $(LINKDYLIB-$(F)-$(A)) $(RTLIBS) $(MYCCLIBS)
	@$(LINKDYLIB-$(F)-$(A)) $(RTLIBS) $(MYCCLIBS)

$(OBJROOT)/$(A)/$(F):
	$(MKDIR) '$(.TARGET)'

.endfor # RC_ARCHS
.endfor # FORMS

SEG1ADDR_i386 = 0xffff0000
SEG1ADDR_ppc = 0xffff8000
SEG1ADDR_ppc64 = 0xffffffffffff8000
SEG1ADDR_x86_64 = 0x00007fffffe00000
SEG1ADDR_arm = 0xffff8000

CFLAGS = -g -Os -Wall -Werror -I'$(SLFS_F_PH)' -fno-common $(SDKROOTCFLAGS)

$(OBJROOT)/System_vers.c:
	$(VERS_STRING) -c System | \
	$(SED) -e 's/SGS_VERS/SYSTEM_VERS_STRING/' -e 's/VERS_NUM/SYSTEM_VERS_NUM/' > $(.TARGET)

.for A in $(RC_ARCHS)
$(OBJROOT)/$(A)/SystemMath.o: $(SRCROOT)/SystemMath.s
	$(MYCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

$(OBJROOT)/$(A)/CompatibilityHacks.o: $(SRCROOT)/CompatibilityHacks.c
	$(MYCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

$(OBJROOT)/$(A)/System_vers.o: $(OBJROOT)/System_vers.c
	$(MYCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

$(OBJROOT)/$(A)/init.o: $(SRCROOT)/init.c
	$(MYCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

.endfor # RC_ARCHS

installhdrs:

.for F in $(FORMS)
BI-install-$(F): build-$(F)
	$(MKDIR) "$(DSTROOT)/usr/lib"
	@$(ECHO) "===== Installing $(NAME)$(SUFFIX$(F)).dylib ====="
	$(INSTALL) "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib" "$(DSTROOT)/usr/lib"
	$(STRIP) -S "$(DSTROOT)/usr/lib/$(NAME)$(SUFFIX$(F)).dylib"
	$(CHMOD) a-w "$(DSTROOT)/usr/lib/$(NAME)$(SUFFIX$(F)).dylib"
	$(LN) -sf "$(NAME)$(SUFFIX$(F)).dylib" "$(DSTROOT)/usr/lib/$(NAME0)$(SUFFIX$(F)).dylib"
	$(CODESIGN) -s - "$(DSTROOT)/usr/lib/$(NAME)$(SUFFIX$(F)).dylib"
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	$(RMDIR) $(OBJROOT)/$(A)
.endfor # RC_ARCHS
