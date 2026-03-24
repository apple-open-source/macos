/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#include <AppleFeatures/AppleFeatures.h>
#include <os/script_config_private.h>
#include <os/security_config_private.h>
#include <os/overflow.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_param.h>
#include <libproc.h>
#include <sys/proc_info_private.h>
#include <_simple.h>
#include "os/internal.h"

OS_NOEXPORT os_security_config_t __security_config;

__attribute__((section("__TPRO_CONST,__data")))
os_security_config_t __security_config = OS_SECURITY_CONFIG_NONE;

/*
 * This region may need to be unmapped and a new, permanent, region mapped
 * in it's place. We will therefore ensure it is both page aligned, and at
 * least a full page size, to ensure no impact on any other region.
 */

typedef union {
	os_security_config_t value;
	char padding[PAGE_MAX_SIZE];
} page_sized_config_t;

__attribute__((section("__DATA,__script_config")))
__attribute__((aligned(PAGE_MAX_SIZE)))
page_sized_config_t __restrictions_config = {
	.value = OS_SECURITY_CONFIG_NONE
};

#define SECURITY_CONFIG_KEY "security_config"


static uint64_t
_parse_security_config_string(const char *str)
{
	const uint64_t base = 16;
	const unsigned char *s = (const unsigned char *)str;
	int any = 0;
	uint64_t acc = 0;

	if (s[0] == '0' && s[1] == 'x') {
		s += 2;
	} else {
		any = -1;
	}

	for (unsigned char c = *s++; c != '\0' && any != -1; c = *s++) {
		if (c >= '0' && c <= '9') {
			c -= '0';
		} else if (c >= 'A' && c <= 'F') {
			c -= 'A' - 10;
		} else if (c >= 'a' && c <= 'f') {
			c -= 'a' - 10;
		} else {
			any = -1;
		}

		if (any >= 0 && c < base) {
			if (os_mul_and_add_overflow(acc, base, c, &acc)) {
				any = -1;
			} else {
				any = 1;
			}
		}
	}

	if (any > 0) {
		return acc;
	}

	_os_set_crash_log_message("Could not parse " SECURITY_CONFIG_KEY " string");
	__builtin_trap();
}


static inline void
_freeze_script_config_storage_as_unusable(void) {
	mach_vm_address_t target_address =
			(mach_vm_address_t) &os_script_config_storage;

	kern_return_t kr;
	kr = mach_vm_map(mach_task_self(), &target_address,
			sizeof(os_script_config_storage), 0,
			VM_FLAGS_PERMANENT | VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
			MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_NONE, VM_PROT_NONE,
			VM_INHERIT_DEFAULT);
	if (unlikely(kr)) {
		__LIBPLATFORM_INTERNAL_CRASH__(kr,
				"Failed to freeze script config storage.");
	}
}


static inline void
_freeze_restrictions_config(os_security_config_t config_value) {
	mach_vm_address_t target_address =
			(mach_vm_address_t) &__restrictions_config;

	kern_return_t kr;
	kr = mach_vm_map(mach_task_self(), &target_address,
			sizeof(__restrictions_config), 0,
			VM_FLAGS_PERMANENT | VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
			MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT,
			VM_INHERIT_DEFAULT);
	if (unlikely(kr)) {
		__LIBPLATFORM_INTERNAL_CRASH__(kr, "Failed to freeze config.");
	}

	__restrictions_config.value = config_value;

	kr = mach_vm_protect(mach_task_self(), target_address, sizeof(__restrictions_config), TRUE, VM_PROT_READ);
	if (unlikely(kr)) {
		__LIBPLATFORM_INTERNAL_CRASH__(kr, "Failed to reprotect config.");
	}
}

static inline os_security_config_t
_os_security_config_masked(void) {
	os_security_config_t mask = OS_SECURITY_CONFIG_HARDENED_HEAP |
			OS_SECURITY_CONFIG_TPRO | OS_SECURITY_CONFIG_GUARD_OBJECTS;
	return __security_config & mask;
}

static inline os_security_config_t
_os_security_config_restrictions_masked(void) {
	os_security_config_t mask = OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS;
	return __restrictions_config.value & mask;
}


__attribute__ ((visibility ("hidden")))
void __os_security_config_init(const char *apple[]);

__attribute__ ((visibility ("hidden")))
void
__os_security_config_init(const char *apple[])
{
	uint64_t value = 0;
	const char *str;
	str = _simple_getenv(apple, SECURITY_CONFIG_KEY);

	if (str != NULL) {
		value = _parse_security_config_string(str);
	}

	if (value & OS_SECURITY_CONFIG_HARDENED_HEAP) {
		__security_config |= OS_SECURITY_CONFIG_HARDENED_HEAP;
	}
	if (value & OS_SECURITY_CONFIG_TPRO) {
		__security_config |= OS_SECURITY_CONFIG_TPRO;
	}

	if (value & OS_SECURITY_CONFIG_GUARD_OBJECTS) {
		__security_config |= OS_SECURITY_CONFIG_GUARD_OBJECTS;
	}

	os_security_config_t restrictions_value = OS_SECURITY_CONFIG_NONE;

	if (value & OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS) {
		restrictions_value |= OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS;

		_freeze_script_config_storage_as_unusable();
	}

	if (restrictions_value != OS_SECURITY_CONFIG_NONE) {
		_freeze_restrictions_config(restrictions_value);
	}
}

os_security_config_t
os_security_config_get(void) {
	return _os_security_config_masked() |
			_os_security_config_restrictions_masked();
}

static inline os_security_config_t
_pbi_flags_to_security_config(uint32_t pbi_flags) {
	os_security_config_t config = OS_SECURITY_CONFIG_NONE;

	if (pbi_flags & PROC_FLAG_HARDENED_HEAP_ENABLED) {
		config |= OS_SECURITY_CONFIG_HARDENED_HEAP;
	}
	if (pbi_flags & PROC_FLAG_TPRO_ENABLED) {
		config |= OS_SECURITY_CONFIG_TPRO;
	}
	if (pbi_flags & PROC_FLAG_GUARD_OBJECTS_ENABLED) {
		config |= OS_SECURITY_CONFIG_GUARD_OBJECTS;
	}
	if (pbi_flags & PROC_FLAG_SCRIPT_RESTRICTIONS_ENABLED) {
		config |= OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS;
	}

	return config;
}

static inline os_security_config_t
_task_security_config_info_to_security_config(uint32_t task_config) {
	os_security_config_t mask = OS_SECURITY_CONFIG_HARDENED_HEAP |
								OS_SECURITY_CONFIG_TPRO |
								OS_SECURITY_CONFIG_GUARD_OBJECTS |
								OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS;

	return (os_security_config_t)task_config & mask;
}


int
os_security_config_get_for_proc(pid_t pid, os_security_config_t *config) {
	struct proc_bsdinfo bsd_info;

	if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info))
			!= sizeof(bsd_info)) {
		*config = OS_SECURITY_CONFIG_NONE;
		return -1;
	}

	*config = _pbi_flags_to_security_config(bsd_info.pbi_flags);
	return 0;
}

int
os_security_config_get_for_task(task_t task, os_security_config_t *config) {
	struct task_security_config_info info = {};
	mach_msg_type_number_t count = TASK_SECURITY_CONFIG_INFO_COUNT;

	if (task_info(task, TASK_SECURITY_CONFIG_INFO, (task_info_t)&info, &count)
			!= KERN_SUCCESS) {
		*config = OS_SECURITY_CONFIG_NONE;
		return -1;
	}

	*config = _task_security_config_info_to_security_config(info.config);
	return 0;
}
