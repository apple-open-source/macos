NAME0 = libSystem
NAME = $(NAME0).$(VersionLetter)

# for now, use the default compiler
GCC := $(CC)
GCCLIBS = -lgcc -lgcc_eh
NARCHS != echo $(RC_ARCHS) | wc -w
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
SLFS_F_PH = $(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
.ifdef SDKROOT
SDKROOTCFLAGS = -isysroot '$(SDKROOT)'
SDKROOTLDFLAGS = -syslibroot '$(SDKROOT)'
.endif
.if !empty(FEATURE_ORDER_FILE)
ORDERFILES = -Wl,-order_file,$(SRCROOT)/SystemInit.order -Wl,-order_file,$(SRCROOT)/System.order
.endif
LIBS = -lc -lcommonCrypto -ldyldapis\
       -linfo -ldns_sd -lm -lmacho\
       -lnotify -lkeymgr -llaunch \
       -lcopyfile -lsandbox -lremovefile
CONDITIONALLIBS = unc quarantine
.for L in $(CONDITIONALLIBS)
# the following should be replaced with tconf when 5784037 is complete
.if exists($(LIBSYS)/lib$(L).a)
LIBS += -l$(L)
.endif
.endfor # CONDITIONALLIBS

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
LIPOARGS$(F) != perl -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/$$_/$(F)/$(NAME)$(SUFFIX$(F)).dylib\"), qw($(RC_ARCHS))))'

.for A in $(RC_ARCHS)
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS
build-$(F):
.if $(NARCHS) == 1
	cp -p "$(OBJROOT)/$(RC_ARCHS)/$(F)/$(NAME)$(SUFFIX$(F)).dylib" "$(SYMROOT)"
.else
	lipo -create $(LIPOARGS$(F)) -output "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib"
.endif
	dsymutil "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib"

.for A in $(RC_ARCHS)
build-$(A)-$(F): $(OBJROOT)/$(A)/$(F) $(OBJROOT)/$(A)/CommPageSymbols.o $(OBJS-$(A))
	$(GCC) -dynamiclib -arch $(A) -pipe \
	    -o '$(OBJROOT)/$(A)/$(F)/$(NAME)$(SUFFIX$(F)).dylib' \
	    -compatibility_version 1 -current_version $(Version) \
	    -install_name /usr/lib/$(NAME)$(SUFFIX$(F)).dylib \
	    -nodefaultlibs -all_load -multi_module -Wl,-search_paths_first \
	    -segcreate __DATA __commpage $(OBJROOT)/$(A)/CommPageSymbols.o \
	    $(ORDERFILES) $(SKDROOTLDFLAGS) $(OBJS-$(A)) \
	    -L$(DSTROOT)/usr/local/lib/system -L$(LIBSYS) $(LIBMATHCOMMON$(F)) \
	    $(LIBS:C/$/$(SUFFIX$(F))/) $(GCCLIBS)

$(OBJROOT)/$(A)/$(F):
	mkdir -p '$(.TARGET)'

.endfor # RC_ARCHS
.endfor # FORMS

SEG1ADDR_i386 = 0xffff0000
SEG1ADDR_ppc = 0xffff8000
SEG1ADDR_ppc64 = 0xffffffffffff8000
SEG1ADDR_x86_64 = 0x00007fffffe00000
SEG1ADDR_arm = 0xffff8000

CFLAGS = -g -Os -Wall -Werror -I'$(SLFS_F_PH)' -fno-common $(SDKROOTCFLAGS)

$(OBJROOT)/System_vers.c:
	vers_string -c System | \
	sed -e 's/SGS_VERS/SYSTEM_VERS_STRING/' -e 's/VERS_NUM/SYSTEM_VERS_NUM/' > $(.TARGET)

.for A in $(RC_ARCHS)
$(OBJROOT)/$(A)/CommPageSymbols.o: $(SRCROOT)/CommPageSymbols.st
	$(GCC) -c -o '$(.TARGET:R)_intermediate.$(.TARGET:E)' -arch $(A) -x assembler-with-cpp $(CFLAGS) '$(.ALLSRC)'
	ld -arch $(A) -r -seg1addr $(SEG1ADDR_$(A:C/^armv.*$/arm/)) '$(.TARGET:R)_intermediate.$(.TARGET:E)' -o '$(.TARGET)'

$(OBJROOT)/$(A)/SystemMath.o: $(SRCROOT)/SystemMath.s
	$(GCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

$(OBJROOT)/$(A)/System_vers.o: $(OBJROOT)/System_vers.c
	$(GCC) -c -o '$(.TARGET)' -arch $(A) $(CFLAGS) '$(.ALLSRC)'

.endfor # RC_ARCHS

installhdrs:

.for F in $(FORMS)
BI-install-$(F): build-$(F)
	mkdir -p "$(DSTROOT)/usr/lib"
	@echo "===== Installing $(NAME)$(SUFFIX$(F)).dylib ====="
	install "$(SYMROOT)/$(NAME)$(SUFFIX$(F)).dylib" "$(DSTROOT)/usr/lib"
	strip -S "$(DSTROOT)/usr/lib/$(NAME)$(SUFFIX$(F)).dylib"
	chmod a-w "$(DSTROOT)/usr/lib/$(NAME)$(SUFFIX$(F)).dylib"
	ln -sf "$(NAME)$(SUFFIX$(F)).dylib" "$(DSTROOT)/usr/lib/$(NAME0)$(SUFFIX$(F)).dylib"
.endfor # FORMS

install-all: build
.for F in $(FORMS)
install-all: BI-install-$(F)
.endfor # FORMS

clean:
.for A in $(RC_ARCHS)
	rm -rf $(OBJROOT)/$(A)
.endfor # RC_ARCHS
