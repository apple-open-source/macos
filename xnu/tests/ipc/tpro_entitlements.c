#include <darwintest.h>
#include <darwintest_utils.h>

#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/mach_vm.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <mach/task.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.spawn"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("spawn"),
	T_META_TAG_VM_PREFERRED);

struct task_security_config {
	uint8_t hardened_heap: 1,
	    tpro :1,
	    reserved: 1,
	    platform_restrictions_version :3;
	uint8_t hardened_process_version;
};

T_DECL(test_platform_restrictions_entitlements,
    "entitlement should enable the platform restrictions configuration",
    T_META_CHECK_LEAKS(false),
    T_META_TAG_VM_NOT_ELIGIBLE,
    T_META_BOOTARGS_SET("amfi=0x7"),
    T_META_ENABLED(false) /* rdar://153306234 */)
{
	struct task_security_config_info config;
	struct task_ipc_space_policy_info space_info;
	mach_msg_type_number_t count;
	kern_return_t kr;

	count = TASK_SECURITY_CONFIG_INFO_COUNT;
	kr = task_info(mach_task_self(), TASK_SECURITY_CONFIG_INFO, (task_info_t)&config, &count);
	T_ASSERT_MACH_SUCCESS(kr, "task_info(TASK_SECURITY_CONFIG_INFO)");
	T_ASSERT_EQ(count, 1, "security config should return 1 value");

	struct task_security_config *conf = (struct task_security_config*)&config;

	T_EXPECT_TRUE(conf->tpro, "TPRO bit should be set");

	T_EXPECT_FALSE(conf->reserved, "reserved bit should not be set");
	T_EXPECT_FALSE(conf->hardened_heap, "hardened_heap bit should not be set");
	uint8_t vers = conf->platform_restrictions_version;
	T_EXPECT_EQ_UINT(vers, 0, "Platform restrictions version should be 0");

	count = TASK_IPC_SPACE_POLICY_INFO_COUNT;
	kr = task_info(mach_task_self(), TASK_IPC_SPACE_POLICY_INFO, (task_info_t)&space_info, &count);
	T_ASSERT_MACH_SUCCESS(kr, "task_info(TASK_SECURITY_CONFIG_INFO)");
	T_ASSERT_EQ_UINT(count, 1, "ipc space should return 1 value");

	T_EXPECT_FALSE(space_info.space_policy & 0x100, "enhanced V0 bit should not be set");
	T_EXPECT_FALSE(space_info.space_policy & 0x200, "enhanced V1 bit should not be set");
	T_EXPECT_FALSE(space_info.space_policy & 0x400, "enhanced V2 bit should not be set");
}
