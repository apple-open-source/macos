PROJECT := libpthread
TEST_DIR := tests/

ENABLE_LTE_TESTS=YES

ifeq ($(DEVELOPER_DIR),)
	DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

TARGETS :=
TARGETS += atfork
TARGETS += bsdthread_set_self
TARGETS += stack
TARGETS += stack_size
TARGETS += cond
#TARGETS += cond_hang3
#TARGETS += cond_stress
TARGETS += cond_timed
TARGETS += cond_prepost
TARGETS += custom_stack
TARGETS += stack_aslr
TARGETS += join
TARGETS += main_stack
TARGETS += main_stack_custom
TARGETS += detach
#TARGETS += maxwidth
TARGETS += mutex
TARGETS += mutex_prepost
TARGETS += mutex_try
TARGETS += once_cancel
TARGETS += pthread_attr_setstacksize
TARGETS += pthread_bulk_create
TARGETS += pthread_cancel
TARGETS += pthread_create_from_mach_thread
TARGETS += pthread_cwd
TARGETS += pthread_exit
TARGETS += pthread_introspection
TARGETS += pthread_setspecific
TARGETS += pthread_threadid_np
TARGETS += pthread_get_qos_class_np
TARGETS += pthread_dependency
#TARGETS += qos
TARGETS += rdar_32848402
#TARGETS += rwlock-22244050
#TARGETS += rwlock-signal
#TARGETS += rwlock
TARGETS += tsd
TARGETS += setrlimit_sigsegv
#TARGETS += wq_block_handoff
#TARGETS += wq_event_manager
#TARGETS += wq_kevent
#TARGETS += wq_kevent_stress
TARGETS += wq_limits
TARGETS += add_timer_termination
TARGETS += perf_contended_mutex_rwlock

EXCLUDED_SOURCES := wq_cooperative

ifeq ($(PLATFORM),MacOSX))
	JIT_LEGACY_SUPPORTED = 1
	JIT_CALLBACK_SUPPORTED = 1
	JIT_ENTITLEMENT_SUFFIX = macos
else ifeq ($(PLATFORM),iPhoneOS)
	JIT_LEGACY_SUPPORTED = 0
	JIT_CALLBACK_SUPPORTED = 1
	JIT_ENTITLEMENT_SUFFIX = ios
else
	JIT_LEGACY_SUPPORTED = 0
	JIT_CALLBACK_SUPPORTED = 0
endif

# rdar://121952211
# ifeq ($(JIT_LEGACY_SUPPORTED),1)
# TARGETS += pthread_jit_write_protection
# pthread_jit_write_protection: CODE_SIGN_ENTITLEMENTS=pthread_jit_write_protection-entitlements.plist
# endif

ifeq ($(JIT_CALLBACK_SUPPORTED), 1)
TARGETS += pthread_jit_write_lockdown
pthread_jit_write_lockdown: CODE_SIGN_ENTITLEMENTS=pthread_jit_write_lockdown-entitlements.plist

OTHER_TEST_TARGETS = pthread_jit_write_with_callback pthread_jit_write_freeze_callbacks

pthread_jit_write_with_callback: pthread_jit_write_with_callback.c libpthreadjittest.notdylib
	$(CC) $(DT_CFLAGS) $(OTHER_CFLAGS) $(CFLAGS) \
			$(DT_LDFLAGS) $(OTHER_LDFLAGS) $(LDFLAGS) \
			pthread_jit_write_with_callback.c $(SYMROOT)/assets/libpthreadjittest.notdylib \
			-rpath @executable_path/assets \
			-o $(SYMROOT)/$@
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none --entitlements pthread_jit_write_with_callback-$(JIT_ENTITLEMENT_SUFFIX)-entitlements.plist $(SYMROOT)/$@

pthread_jit_write_freeze_callbacks: pthread_jit_write_freeze_callbacks.c libpthreadjittest.notdylib
	$(CC) $(DT_CFLAGS) $(OTHER_CFLAGS) $(CFLAGS) \
			$(DT_LDFLAGS) $(OTHER_LDFLAGS) $(LDFLAGS) \
			pthread_jit_write_freeze_callbacks.c \
			-o $(SYMROOT)/$@
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none --entitlements pthread_jit_write_freeze_callbacks-$(JIT_ENTITLEMENT_SUFFIX)-entitlements.plist $(SYMROOT)/$@
endif

# this should be CUSTOM_TARGETS, see "Compatibility defines" in Makefile.targets
OTHER_TARGETS := stackoverflow_crash
OTHER_TARGETS += wq_cooperative

ifeq ($(JIT_CALLBACK_SUPPORTED),1)
OTHER_TARGETS += pthread_jit_write_with_callback_tool \
		pthread_jit_write_freeze_callbacks_tool \
		libpthreadjittest.notdylib
endif

BATS_PLISTS += $(SRCROOT)/com.apple.libpthread.tests.plist

OTHER_LTE_INCLUDE_FILES += \
	/usr/local/lib/libdarwintest_utils.dylib

OTHER_CFLAGS := -DDARWINTEST -Weverything \
		-Wno-vla -Wno-bad-function-cast -Wno-missing-noreturn \
		-Wno-missing-field-initializers -Wno-format-pedantic \
		-Wno-gnu-folding-constant -Wno-used-but-marked-unused \
		-Wno-padded -Werror=undef
OTHER_LDFLAGS := -ldarwintest_utils

#TARGETS += main_stack_legacy // Disabled by default due to linker warnings
#main_stack_legacy: OTHER_LDFLAGS += -Wl,-stack_addr,0xc1000000 -Wl,-stack_size,0x0f00000
#main_stack_legacy: OTHER_CFLAGS += -DSTACKSIZE=0x0f00000
#main_stack_legacy: ARCH_FLAGS = -arch i386
#main_stack_legacy: DEPLOYMENT_TARGET_FLAGS = -mmacosx-version-min=10.7

main_stack_custom: OTHER_LDFLAGS += -Wl,-stack_size,0x124000
main_stack_custom: OTHER_CFLAGS += -DSTACKSIZE=0x124000

bsdthread_set_self: OTHER_CFLAGS += -D_DARWIN_FEATURE_CLOCK_GETTIME
wq_cooperative: OTHER_CFLAGS += -Wno-atomic-implicit-seq-cst -Wno-reserved-id-macro -Wno-unused-function

rdar_32848402: OTHER_CFLAGS += -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

stackoverflow_crash: helpers/stackoverflow_crash.c
	mkdir -p $(SYMROOT)/assets/
	$(CC) -o $(SYMROOT)/assets/$@ $(CFLAGS) -D_POSIX_C_SOURCE=1 $(OTHER_CFLAGS) $(LDFLAGS) $(OTHER_LDFLAGS)  $<
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none $(SYMROOT)/assets/$@

install-stackoverflow_crash: stackoverflow_crash
	mkdir -p $(INSTALLDIR)/assets
	@cp $(SYMROOT)/assets/stackoverflow_crash $(INSTALLDIR)/assets

wq_cooperative: $(SRCROOT)/wq_cooperative.c
	$(CC) $(DT_CFLAGS) $(OBJC_CFLAGS) $(OTHER_CFLAGS) $(CFLAGS) $(DT_LDFLAGS) $(OTHER_LDFLAGS) $(OBJC_LDFLAGS) $(LDFLAGS) $^ -o $(SYMROOT)/$@
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - -o library --timestamp=none $(SYMROOT)/$@

install-wq_cooperative: wq_cooperative
	mkdir -p $(INSTALLDIR)
	@cp $(SYMROOT)/wq_cooperative $(INSTALLDIR)/wq_cooperative
	@cp -R $(SYMROOT)/wq_cooperative.dSYM $(INSTALLDIR)/

ifeq ($(JIT_CALLBACK_SUPPORTED),1)
pthread_jit_write_with_callback_tool: helpers/pthread_jit_write_with_callback_tool.c
	mkdir -p $(SYMROOT)/assets/
	$(CC) -o $(SYMROOT)/assets/$@ $(CFLAGS) $(OTHER_CFLAGS) $(LDFLAGS) $(OTHER_LDFLAGS) $<
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none --entitlements pthread_jit_write_with_callback-$(JIT_ENTITLEMENT_SUFFIX)-entitlements.plist $(SYMROOT)/assets/pthread_jit_write_with_callback_tool

install-pthread_jit_write_with_callback_tool: pthread_jit_write_with_callback_tool
	mkdir -p $(INSTALLDIR)/assets
	@cp $(SYMROOT)/assets/pthread_jit_write_with_callback_tool $(INSTALLDIR)/assets

pthread_jit_write_freeze_callbacks_tool: helpers/pthread_jit_write_with_callback_tool.c
	mkdir -p $(SYMROOT)/assets/
	$(CC) -o $(SYMROOT)/assets/$@ $(CFLAGS) $(OTHER_CFLAGS) $(LDFLAGS) $(OTHER_LDFLAGS) $<
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none --entitlements pthread_jit_write_freeze_callbacks-$(JIT_ENTITLEMENT_SUFFIX)-entitlements.plist $(SYMROOT)/assets/pthread_jit_write_freeze_callbacks_tool

install-pthread_jit_write_freeze_callbacks_tool: pthread_jit_write_freeze_callbacks_tool
	mkdir -p $(INSTALLDIR)/assets
	@cp $(SYMROOT)/assets/pthread_jit_write_freeze_callbacks_tool $(INSTALLDIR)/assets

# .notdylib to dodge verifiers :)
libpthreadjittest.notdylib: helpers/libpthreadjittest.c
	mkdir -p $(SYMROOT)/assets/
	$(CC) -o $(SYMROOT)/assets/$@ -dynamiclib $(CFLAGS) $(OTHER_CFLAGS) $(LDFLAGS) $(OTHER_LDFLAGS) -install_name @rpath/libpthreadjittest.notdylib $<
	env CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) $(CODESIGN) --force --sign - --timestamp=none $(SYMROOT)/assets/libpthreadjittest.notdylib

install-libpthreadjittest.notdylib: libpthreadjittest.notdylib
	mkdir -p $(INSTALLDIR)/assets
	@cp $(SYMROOT)/assets/libpthreadjittest.notdylib $(INSTALLDIR)/assets
endif
