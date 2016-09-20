PROJECT := libpthread
TEST_DIR := tests/

ifeq ($(DEVELOPER_DIR),)
	DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

TARGETS :=
TARGETS += atfork
TARGETS += bsdthread_set_self
TARGETS += cond
#TARGETS += cond_hang3
#TARGETS += cond_stress
TARGETS += cond_timed
TARGETS += custom_stack
TARGETS += stack_aslr
TARGETS += join
TARGETS += main_stack
TARGETS += main_stack_custom
#TARGETS += maxwidth
TARGETS += mutex
TARGETS += mutex_try
TARGETS += once_cancel
TARGETS += pthread_attr_setstacksize
TARGETS += pthread_bulk_create
TARGETS += pthread_cancel
TARGETS += pthread_cwd
TARGETS += pthread_exit
TARGETS += pthread_introspection
TARGETS += pthread_setspecific
TARGETS += pthread_threadid_np
#TARGETS += qos
#TARGETS += rwlock-22244050
#TARGETS += rwlock-signal
#TARGETS += rwlock
TARGETS += tsd
#TARGETS += wq_block_handoff
#TARGETS += wq_event_manager
#TARGETS += wq_kevent
#TARGETS += wq_kevent_stress
TARGETS += add_timer_termination

OTHER_TARGETS :=

OTHER_CFLAGS := -DDARWINTEST -Weverything \
		-Wno-vla -Wno-bad-function-cast -Wno-missing-noreturn \
		-Wno-missing-field-initializers -Wno-format-pedantic \
		-Wno-gnu-folding-constant
OTHER_LDFLAGS := -ldarwintest_utils

#TARGETS += main_stack_legacy // Disabled by default due to linker warnings
#main_stack_legacy: OTHER_LDFLAGS += -Wl,-stack_addr,0xc1000000 -Wl,-stack_size,0x0f00000
#main_stack_legacy: OTHER_CFLAGS += -DSTACKSIZE=0x0f00000
#main_stack_legacy: ARCH_FLAGS = -arch i386
#main_stack_legacy: DEPLOYMENT_TARGET_FLAGS = -mmacosx-version-min=10.7

main_stack_custom: OTHER_LDFLAGS += -Wl,-stack_size,0x14000
main_stack_custom: OTHER_CFLAGS += -DSTACKSIZE=0x14000

bsdthread_set_self: OTHER_CFLAGS += -D_DARWIN_FEATURE_CLOCK_GETTIME

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
