#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <darwintest.h>
#include <sys/sysctl.h>
#include <TargetConditionals.h>
#include "test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_TIMEOUT(10),
	T_META_RUN_CONCURRENTLY(TRUE));

T_DECL(ipcpv_telemetry_test,
    "Make sure there is no telemetry for known issues during presub testing",
    T_META_CHECK_LEAKS(false),
    XNU_T_META_REQUIRES_DEVELOPMENT_KERNEL,
    T_META_TAG_VM_PREFERRED)
{
	unsigned int telemetry_count = 0;
	size_t telemetry_size = sizeof(telemetry_count);
	int kr = sysctlbyname("debug.ipcpv_telemetry_count", &telemetry_count, &telemetry_size, NULL, 0);
	if (kr != 0) {
		T_SKIP("sysctl debug.ipcpv_telemetry_count");
		exit(0);
	}
	T_ASSERT_EQ(kr, 0, "sysctl debug.ipcpv_telemetry_count");
	T_ASSERT_EQ(telemetry_count, 0, "found ipc policy violation during presub");
}
