.if defined(CCARCH) && ${CCARCH} == armv6
CFLAGS += -mthumb
.endif

_arm_arch_6 != ${MYCC} -E -dD ${CFLAGS:M-[BIDFU]*} ${AINC} -include arm/arch.h -x c /dev/null | grep -q -w _ARM_ARCH_6 || echo NO
.if ${_arm_arch_6} != NO
FEATURE_ARM_ARCH_6 = 1
.endif
