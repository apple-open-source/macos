#include <darwintest.h>
#include <darwintest_utils.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach/vm_region.h>
#include <mach/mach_vm.h>
#include <sys/kern_sysctl.h>
#include <errno.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"));

static const char* g_sysctl_name = "vm.get_owned_vmobjects";

mach_port_t get_corpse(void);

mach_port_t
get_corpse()
{
	kern_return_t kr;
	mach_port_t corpse_port;

	kr = task_generate_corpse(mach_task_self(), &corpse_port);
	if (kr != KERN_SUCCESS) {
		mach_error("task_generate_corpse failed", kr);
		corpse_port = MACH_PORT_NULL;

		switch (kr) {
		case KERN_NOT_SUPPORTED:
		case KERN_FAILURE:
		case KERN_RESOURCE_SHORTAGE:
			break;
		default:
			/* convert to KERN_FAILURE after logging to catch other rc codes */
			/* and trigger test failure */
			kr = KERN_FAILURE;
			break;
		}
	}

	/* anything other than KERN_FAILURE is valid */
	T_EXPECT_NE(kr, KERN_FAILURE, "corpse creation\n");

	return corpse_port;
}

static void
main_test(void)
{
	int ret;
	mach_port_name_t task_name;
	vmobject_list_output_t out_buffer;
	size_t out_size;
	size_t output_size;
	const vm_size_t tmp_size = 16 * 1024 * 1024; /* arbitrary size */
	vm_address_t tmp_buf;
	vm_address_t tmp_buf2;
	mach_vm_size_t addr_size;
	mach_vm_address_t addr;
	kern_return_t kr;
	mach_port_t __self = mach_task_self();
	vm_region_submap_info_data_64_t regionInfo;
	uint32_t nestingDepth;
	mach_msg_type_number_t count;

	/* allocate a temporary buffer */
	kr = vm_allocate(__self, &tmp_buf, tmp_size, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "vm_allocate(%zu) error 0x%x (%s)",
	    (size_t) tmp_size, kr, mach_error_string(kr));
	T_QUIET;
	T_EXPECT_NE(tmp_buf, (vm_address_t) 0, "failed to allocate temporary purgable buffer\n");

	kr = vm_allocate(__self, &tmp_buf2, tmp_size, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "vm_allocate(%zu) error 0x%x (%s)",
	    (size_t) tmp_size, kr, mach_error_string(kr));
	T_QUIET;
	T_EXPECT_NE(tmp_buf2, (vm_address_t) 0, "failed to allocate temporary purgable buffer\n");

	/* expected failures */
	out_size = tmp_size;
	ret = sysctlbyname(g_sysctl_name, NULL, 0, NULL, 0);
	T_EXPECT_EQ(ret, -1, "expected failure with 0 parameters\n");
	T_EXPECT_EQ(errno, EINVAL, "expected EINVAL with 0 parameters\n");

	ret = sysctlbyname(g_sysctl_name, (void*) tmp_buf, &out_size, NULL, 0);
	T_EXPECT_EQ(ret, -1, "expected failure with no new parameters\n");
	T_EXPECT_EQ(errno, EINVAL, "expected EINVAL with 0 new parameters\n");

	out_size = tmp_size;
	ret = sysctlbyname(g_sysctl_name, NULL, 0, (void*) tmp_buf, out_size);
	T_EXPECT_EQ(ret, -1, "expected failure with no old parameters\n");
	T_EXPECT_EQ(errno, EINVAL, "expected EINVAL with 0 old parameters\n");

	task_name = MACH_PORT_NULL;
	ret = sysctlbyname(g_sysctl_name, (void*) tmp_buf, &out_size, &task_name, sizeof(task_name));
	T_EXPECT_EQ(ret, -1, "expected failure with task_name == MACH_PORT_NULL in new parameters\n");
	T_EXPECT_EQ(errno, ESRCH, "expected ESRCH with invalid task port name\n");

	out_size = 0;
	task_name = __self;
	ret = sysctlbyname(g_sysctl_name, (void*) tmp_buf, &out_size, &task_name, sizeof(task_name));
	T_QUIET;
	T_EXPECT_EQ(ret, -1, "expected failure with out_size == 0\n");
	T_EXPECT_EQ(errno, EINVAL, "expected EINVAL with 0 output size and valid pointer\n");

	/* we should get the number of entries we should allocate for */
	out_size = 0;
	output_size = 0;
	ret = sysctlbyname(g_sysctl_name, NULL, &out_size, &task_name, sizeof(task_name));
	T_QUIET;
	T_EXPECT_EQ(ret, 0, "failed getting the number of entries\n");
	T_EXPECT_EQ(out_size, 2 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "expected one entry\n");

	/* calculcate and allocate the proper sized output buffer */
	output_size = out_size;
	out_buffer = (vmobject_list_output_t)calloc(output_size, 1);
	T_QUIET;
	T_EXPECT_NE(out_buffer, NULL, "failed to allocate the output buffer for sysctlbyname\n");

	/* get the truncated list for the current process */
	memset(out_buffer, 0, output_size);
	out_size = 1 * sizeof(vm_object_query_data_t) + sizeof(int64_t);
	ret = sysctlbyname(g_sysctl_name, out_buffer, &out_size, &task_name, sizeof(task_name));

	T_QUIET;
	T_EXPECT_EQ(ret, 0, "sysctlbyname failed\n");
	T_EXPECT_EQ(out_size, 1 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "sysctl return size is incorrect\n");
	T_EXPECT_EQ(out_buffer->entries, 1ULL, "should have 1 vm object\n");
	T_EXPECT_NE(out_buffer->data[0].object_id, 0ULL, "vm_object_id should not be 0\n");

	/* get the list for the current process with an overly large size */
	out_size = SIZE_MAX;
	memset(out_buffer, 0, output_size);
	ret = sysctlbyname(g_sysctl_name, out_buffer, &out_size, &task_name, sizeof(task_name));

	T_QUIET;
	T_EXPECT_EQ(ret, 0, "sysctlbyname failed\n");
	T_EXPECT_EQ(out_size, 2 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "sysctl return size is incorrect\n");
	T_EXPECT_EQ(out_buffer->entries, 2ULL, "should have 2 vm objects\n");
	T_EXPECT_NE(out_buffer->data[0].object_id, 0ULL, "vm_object_id should not be 0\n");

	/* get the list for the current process with the correct output size */
	out_size = output_size;
	memset(out_buffer, 0, output_size);
	ret = sysctlbyname(g_sysctl_name, out_buffer, &out_size, &task_name, sizeof(task_name));

	T_QUIET;
	T_EXPECT_EQ(ret, 0, "sysctlbyname failed\n");
	T_EXPECT_EQ(out_size, 2 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "sysctl return size is incorrect\n");
	T_EXPECT_EQ(out_buffer->entries, 2ULL, "should have 2 vm objects\n");
	T_EXPECT_NE(out_buffer->data[0].object_id, 0ULL, "vm_object_id should not be 0\n");

	addr = tmp_buf;
	addr_size = tmp_size;
	nestingDepth = UINT_MAX;
	count = VM_REGION_SUBMAP_INFO_V2_COUNT_64;
	kr = mach_vm_region_recurse(__self, &addr, &addr_size, &nestingDepth, (vm_region_info_t)&regionInfo, &count);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "mach_vm_region_recurse(%zu) error 0x%x (%s)\n",
	    tmp_size, kr, mach_error_string(kr));
	T_EXPECT_EQ(regionInfo.object_id_full, out_buffer->data[0].object_id, "object_id_full does not match out_buffer->object[0]\n");

	addr = tmp_buf2;
	addr_size = tmp_size;
	nestingDepth = UINT_MAX;
	count = VM_REGION_SUBMAP_INFO_V2_COUNT_64;
	kr = mach_vm_region_recurse(__self, &addr, &addr_size, &nestingDepth, (vm_region_info_t)&regionInfo, &count);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "mach_vm_region_recurse(%zu) error 0x%x (%s)\n",
	    tmp_size, kr, mach_error_string(kr));
	T_EXPECT_EQ(regionInfo.object_id_full, out_buffer->data[1].object_id, "object_id_full does not match out_buffer->object[1]\n");

	/* corpse */
	{
		mach_port_t corpse_port = get_corpse();

		/* corpse_port can be a valid NULL if out of resources, corpse limit, or corpses disabled */
		if (corpse_port != MACH_PORT_NULL) {
			vmobject_list_output_t corpse_out_buffer;
			size_t corpse_out_size;
			size_t corpse_output_size;

			/* we should get the number of entries we should allocate for */
			corpse_out_size = 0;
			ret = sysctlbyname(g_sysctl_name, NULL, &corpse_out_size, &corpse_port, sizeof(corpse_port));

			T_QUIET;
			T_EXPECT_EQ(ret, 0, "failed getting the number of entries for corpse\n");
			T_EXPECT_EQ(corpse_out_size, out_size, "corpse output size matchrd the parent process\n");
			T_EXPECT_EQ(corpse_out_size, 2 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "corpse expected one entry\n");

			/* calculcate and allocate the proper sized output buffer */
			corpse_output_size = corpse_out_size;
			corpse_out_buffer = (vmobject_list_output_t)calloc(corpse_output_size, 1);
			T_QUIET;
			T_EXPECT_NE(corpse_out_buffer, NULL, "failed to allocate the output buffer for sysctlbyname for corpse\n");

			/* get the list for the current process */
			corpse_out_size = corpse_output_size;
			memset(corpse_out_buffer, 0, corpse_output_size);
			ret = sysctlbyname(g_sysctl_name, corpse_out_buffer, &corpse_out_size, &corpse_port, sizeof(corpse_port));

			int rc = memcmp(corpse_out_buffer, out_buffer, corpse_out_size);

			T_QUIET;
			T_EXPECT_EQ(ret, 0, "corpse sysctlbyname failed\n");
			T_EXPECT_EQ(rc, 0, "corpse vmobjects should match parent vmobjects\n");
			T_EXPECT_EQ(corpse_out_size, 2 * sizeof(vm_object_query_data_t) + sizeof(int64_t), "corpse sysctl return size is incorrect\n");
			T_EXPECT_EQ(corpse_out_buffer->entries, 2ULL, "corpse should have 2 vm objects\n");
			T_EXPECT_NE(corpse_out_buffer->data[0].object_id, 0ULL, "corpse vm_object_id should not be 0\n");

			free(corpse_out_buffer);
			mach_port_deallocate(mach_task_self(), corpse_port);
		}
	}

	kr = vm_deallocate(__self, tmp_buf, tmp_size);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "vm_deallocate(%zu) error 0x%x (%s)\n",
	    tmp_size, kr, mach_error_string(kr));

	kr = vm_deallocate(__self, tmp_buf2, tmp_size);
	T_QUIET;
	T_EXPECT_EQ(kr, KERN_SUCCESS, "vm_deallocate(%zu) error 0x%x (%s)\n",
	    tmp_size, kr, mach_error_string(kr));

	free(out_buffer);
	out_buffer = NULL;
}

T_DECL(test_get_vmobject_list, "Get owned vm_objects for process", T_META_TAG_VM_PREFERRED)
{
	main_test();
}
