/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#include "internal.h"

#pragma mark Utilities

static void
_enum_boot_arg_values(char *argsbuff, void *context, os_boot_arg_enumerator_t fp)
{
	bool keep_going = true;
	char *token = NULL;
	char *argsstr = argsbuff;
	static const char seps[] = { ' ', '\t', 0 };
	while (keep_going && (token = strsep(&argsstr, seps)) != NULL) {
		bool is_boolean = false;

		const char *value = NULL;
		char *equals = strchr(token, '=');
		if (token[0] == '-') {
			/*
			 * Arguments whose names begins with "-" are booleans, so don't get
			 * key=value splitting.  Though I'd still discourage you from
			 * naming your option "-edge=case".
			 */
			is_boolean = true;
		} else if (equals) {
			equals[0] = '\0';
			value = &equals[1];
		} else {
			/*
			 * Careful reading of getval() and bootargs.c in the xnu repo says
			 * that a boot argument of the form "foo" (instead of "foo=bar" or
			 * "-foo") is equivalent to "foo=1".
			 */
			value = "1";
		}

		keep_going = fp(context, token, value, is_boolean);
	}
}

typedef struct _boot_arg_context {
	const char *which;
	char *where;
	size_t max;
	bool is_boolean;
	bool found;
} _boot_arg_context;

static bool
_check_boot_arg_value(void *context, const char *which, const char *value, bool is_boolean)
{
	_boot_arg_context *ctx = (_boot_arg_context *)context;

	if (strcmp(ctx->which, which) == 0) {
		/*
		 * Found it! Copy out the value as required.
		 */
		ctx->found = true;

		ctx->is_boolean = is_boolean;
		if (!ctx->where) {
			// Caller just wants to know whether the boot-arg exists.
		} else if (is_boolean || value == NULL) {
			// XNU currently does NOT convert a flag to the string "1", but it
			// does convert it to the integer 1.
			strlcpy(ctx->where, "", ctx->max);
		} else {
			strlcpy(ctx->where, value, ctx->max);
		}
	}

	return !ctx->found;
}

/*
 * Factored out from _get_parse_boot_arg_value for unit testing purposes
 */
static bool
_parse_boot_arg_value(char *argsbuff, const char *which, char *where, size_t max, bool *out_is_boolean)
{
	_boot_arg_context context;
	context.which = which;
	context.where = where;
	context.max = max;
	context.found = false;
	context.is_boolean = false;

	_enum_boot_arg_values(argsbuff, &context, &_check_boot_arg_value);

	if (out_is_boolean) {
		*out_is_boolean = context.is_boolean;
	}

	return context.found;
}

static bool
_parse_boot_arg_int(char *buffer, const char *which, int64_t *where)
{
	bool found = false;
	bool is_boolean = false;
	char buff[24] = {0};
	int64_t val = 0;

	found = _parse_boot_arg_value(buffer, which, buff, sizeof(buff), &is_boolean);
	if (!found || !where) {
		goto __out;
	}

	if (is_boolean) {
		// The kernel treats boolean values as having integer values of 1 when
		// asked about them. See PE_parse_boot_argn_internal() in bootargs.c.
		*where = 1;
	} else if (os_boot_arg_string_to_int(buff, &val)) {
		*where = val;
	} else {
		// The boot-arg value was invalid, so say we didn't find it.
		found = false;
	}

__out:
	return found;
}

/*
 * This is (very) loosely based on the implementation of
 * PE_parse_boot_argn() (or at least the parts where I was able to easily
 * decipher the policy).
 */
static char *_copy_boot_args(void)
{
	char *argsbuff = NULL;
	size_t argsbuff_len = 0;
	(void)sysctlbyname_get_data_np("kern.bootargs",
			(void **)&argsbuff, &argsbuff_len);

	return argsbuff;
}

#pragma mark API
errno_t
sysctl_get_data_np(int mib[4], size_t mib_cnt, void **buff, size_t *buff_len)
{
	errno_t error = 0;
	int ret = 0;
	size_t needed = 0;
	void *mybuff = NULL;

	// We need to get the length of the parameter so we can allocate a buffer
	// that's large enough.
	ret = sysctl(mib, (unsigned int)mib_cnt, NULL, &needed, NULL, 0);
	if (ret) {
		error = errno;
		goto __out;
	}

	mybuff = malloc(needed);
	if (!mybuff) {
		error = errno;
		goto __out;
	}

	ret = sysctl(mib, (unsigned int)mib_cnt, mybuff, &needed, NULL, 0);
	if (ret) {
		// It's conceivable that some other process came along within this
		// window and modified the variable to be even larger than we'd
		// previously been told, but if that's the case, just give up.
		error = errno;
		goto __out;
	}

	*buff = mybuff;
	*buff_len = needed;

__out:
	if (error) {
		free(mybuff);
	}
	return error;
}

errno_t
sysctlbyname_get_data_np(const char *mibdesc, void **buff, size_t *buff_len)
{
	int ret = -1;
	int error = -1;
	int mib[4];
	size_t mib_cnt = countof(mib);

	ret = sysctlnametomib(mibdesc, mib, &mib_cnt);
	if (ret) {
		error = errno;
		goto __out;
	}

	error = sysctl_get_data_np(mib, mib_cnt, buff, buff_len);

__out:
	return error;
}

bool
os_parse_boot_arg_int(const char *which, int64_t *where)
{
	__os_free char *argsbuff = _copy_boot_args();
	return _parse_boot_arg_int(argsbuff, which, where);
}

bool
os_parse_boot_arg_string(const char *which, char *where, size_t maxlen)
{
	__os_free char *argsbuff = _copy_boot_args();
	return _parse_boot_arg_value(argsbuff, which, where, maxlen, NULL);
}

bool
os_parse_boot_arg_from_buffer_int(const char *buffer, const char *which, int64_t *where)
{
	__os_free char *argsbuff = strdup(buffer);
	return _parse_boot_arg_int(argsbuff, which, where);
}

bool
os_parse_boot_arg_from_buffer_string(const char *buffer, const char *which, char *where, size_t maxlen)
{
	__os_free char *argsbuff = strdup(buffer);
	return _parse_boot_arg_value(argsbuff, which, where, maxlen, NULL);
}

bool
os_boot_arg_string_to_int(const char *value, int64_t *out_value)
{
	bool result = false;
	char *endptr = NULL;
	int64_t val;

	// A base of zero handles bases 8, 10, and 16.
	val = strtoll(value, &endptr, 0);
	if (*endptr == 0) {
		*out_value = val;
		result = true;
	}

	return result;
}

void
os_enumerate_boot_args(void *context, os_boot_arg_enumerator_t fp)
{
	__os_free char *argsbuff = _copy_boot_args();
	_enum_boot_arg_values(argsbuff, context, fp);
}

void
os_enumerate_boot_args_from_buffer(const char *buffer, void *context, os_boot_arg_enumerator_t fp)
{
	__os_free char *argsbuff = strdup(buffer);
	_enum_boot_arg_values(argsbuff, context, fp);
}

#ifdef __BLOCKS__
static bool
_visit_boot_args_enumerator_block(void *context, const char *which, const char *value, bool is_boolean)
{
	os_boot_arg_enumerator_b_t block = (os_boot_arg_enumerator_b_t)context;
	return block(which, value, is_boolean);
}

void
os_enumerate_boot_args_b(OS_NOESCAPE os_boot_arg_enumerator_b_t block)
{
	__os_free char *argsbuff = _copy_boot_args();
	os_enumerate_boot_args_from_buffer(argsbuff, block, &_visit_boot_args_enumerator_block);
}

void
os_enumerate_boot_args_from_buffer_b(const char *buffer, OS_NOESCAPE os_boot_arg_enumerator_b_t block)
{
	os_enumerate_boot_args_from_buffer(buffer, block, &_visit_boot_args_enumerator_block);
}
#endif /* __BLOCKS__ */
