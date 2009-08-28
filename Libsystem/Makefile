NAME0 = libSystem
NAME = $(NAME0).$(VersionLetter)

.include <CoreOS/Standard/Commands.mk>
.include <CoreOS/Standard/Variables.mk>

# for now, use the default compiler
MYCC := $(CC)
MYCCLIBS = -lgcc
RTLIBS = -lcompiler_rt
NARCHS != $(ECHO) $(RC_ARCHS) | $(WC) -w
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
SLFS_F_PH = $(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
.ifdef SDKROOT
SDKROOTCFLAGS = -isysroot '$(SDKROOT)'
SDKROOTLDFLAGS = -Wl,-syslibroot,'$(SDKROOT)'
.endif
ORDERFILES = -Wl,-order_file,$(SRCROOT)/SystemInit.order -Wl,-order_file,$(PLATFORM_ORDER_FILE)
LIBS = -lc -lcommonCrypto -ldyldapis\
       -linfo -ldns_sd -lm -lmacho\
       -lnotify -lkeymgr -llaunch \
       -lcopyfile -lremovefile
CONDITIONALLIBS = unc sandbox quarantine closure cache dispatch unwind \
	dnsinfo
LIBSCONDITIONAL != for L in $(CONDITIONALLIBS); do tconf -q --test usr_local_lib_system_Archive:lib$$L && $(ECHO) -l$$L; done

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
OBJS-$(A) = $(OBJROOT)/$(A)/SystemMath.o $(OBJROOT)/$(A)/System_vers.o
.endfor # RC_ARCHS

.for F in $(FORMS)
.if $(dynamic) == $(F)
SUFFIX$(F) =
.else
SUFFIX$(F) = _$(F)
.endif
.if !empty(FEATURE_LIBMATHCOMMON)
LIBMATHCOMMON$(F) = -L/usr/lib/system -sub_library libmathCommon$(SUFFIX$(F)) -lmathCommon$(SUFFIX$(F))
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
LINKDYLIB-$(F)-$(A) = $(MYCC) -dynamiclib -arch $(A) -pipe $(SDKROOTLDFLAGS) \
	-o '$(OBJROOT)/$(A)/$(F)/$(NAME)$(SUFFIX$(F)).dylib' \
	-compatibility_version 1 -current_version $(Version) \
	-install_name /usr/lib/$(NAME)$(SUFFIX$(F)).dylib \
	-nodefaultlibs -all_load -multi_module -Wl,-search_paths_first \
	-segcreate __DATA __commpage $(OBJROOT)/$(A)/CommPageSymbols.o \
	$(ORDERFILES) $(SKDROOTLDFLAGS) $(OBJS-$(A)) \
	-L$(DSTROOT)/usr/local/lib/system -L$(LIBSYS) $(LIBMATHCOMMON$(F)) \
	$(LIBS:C/$/$(SUFFIX$(F))/) $(LIBSCONDITIONAL:C/$/$(SUFFIX$(F))/)

build-$(A)-$(F): $(OBJROOT)/$(A)/$(F) $(OBJROOT)/$(A)/CommPageSymbols.o $(OBJS-$(A))
	@$(ECHO) $(LINKDYLIB-$(F)-$(A)) $(RTLIBS) && \
	if $(LINKDYLIB-$(F)-$(A)) $(RTLIBS); then \
	    $(ECHO) -n; \
	else \
	    $(ECHO) '*** Failed.  Retrying with -lgcc ***' && \
	    $(ECHO) $(LINKDYLIB-$(F)-$(A)) $(MYCCLIBS) && \
	    $(LINKDYLIB-$(F)-$(A)) $(MYCCLIBS); \
	fi

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
$(OBJROOT)/$(A)/CommPageSymbols.o: $(SRCROOT)/CommPageSymbols.st
	$(MYCC) -c -o '$(.TARGET:R)_intermediate.$(.TARGET:E)' -arch $(A) -x assembler-with-cpp $(CFLAGS) '$(.ALLSRC)'
	$(LD) -arch $(A) -r -seg1addr $(SEG1ADDR_$(A:C/^armv.*$/arm/)) '$(.TARGET:R)_intermediate.$(.TARGET:E)' -o '$(.TARGET)'

$(OBJROOT)/$(A)/SystemMath.o: $(SRCROOT)/SystemMath.s
	$(MYCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

$(OBJROOT)/$(A)/System_vers.o: $(OBJROOT)/System_vers.c
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
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	$(RMDIR) $(OBJROOT)/$(A)
.endfor # RC_ARCHS
