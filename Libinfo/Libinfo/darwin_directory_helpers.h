//
// Copyright (c) 2018 Apple Inc. All rights reserved.
//

//
// This header file contains helper functions used by the Darwin Directory
// functions in the lookup and membership sub-projects.  This is a project
// internal header; none of these functions are exported in libsystem.
//

#ifndef DARWIN_DIRECTORY_HELPERS_H
#define DARWIN_DIRECTORY_HELPERS_H

#include <DarwinDirectory/RecordStore_priv.h>
#include <System/machine/cpu_capabilities.h>
#include <TargetConditionals.h>
#include <mach/mach_host.h>
#include <os/assumes.h>
#include <os/reason_private.h>
#include <sys/reason.h>
#include <uuid/uuid.h>

OS_ASSUME_NONNULL_BEGIN

#pragma mark - Support

OS_ALWAYS_INLINE OS_NORETURN
static inline void
_dd_fatal_error(const char *message)
{
	abort_with_reason(OS_REASON_LIBSYSTEM, OS_REASON_LIBSYSTEM_CODE_FAULT, message, 0);
}

#if !TARGET_OS_OSX
OS_ALWAYS_INLINE
static inline uint32_t
_dd_get_multiuser_config_flags(void)
{
	uint32_t value = 0;
	kern_return_t kr = host_get_multiuser_config_flags(mach_host_self(), &value);
	if (os_unlikely(kr != KERN_SUCCESS)) {
		_dd_fatal_error("Darwin Directory unable to look up multiuser config flags");
	}
	return value;
}

OS_ALWAYS_INLINE
static inline bool
_dd_is_shared_ipad(void)
{
	return (_dd_get_multiuser_config_flags() & kIsMultiUserDevice) != 0;
}

OS_ALWAYS_INLINE
static inline uid_t
_dd_get_foreground_uid(void)
{
	return _dd_get_multiuser_config_flags() & kMultiUserCurrentUserMask;
}
#endif // !TARGET_OS_OSX

#pragma mark Appliers

typedef void (^_dd_record_applier_t)(darwin_directory_record_t record, bool * _Nonnull stop);

#define _dd_foreach_record(type, applier) DarwinDirectoryRecordStoreApply(type, applier)

OS_ALWAYS_INLINE
static inline void
_dd_foreach_record_with_id(darwin_directory_type_t type, id_t id, _dd_record_applier_t applier)
{
	// 501 on embedded is a special record id:
	//
	// * Prior to the comm page being initialized, it behaves normally.
	// * After the comm page has been initialized on Shared iPad, lookups of
	//   501 may get redirected to the foreground user's id.

	struct darwin_directory_filter_s filter = {
		.filterBy = DARWIN_DIRECTORY_FILTER_BY_ID,
		.filterValue.id = id,
	};

	__block bool found = false;
	DarwinDirectoryRecordStoreApplyWithFilter(type, &filter, ^(darwin_directory_record_t record, bool *stop) {
		applier(record, stop);
		found = true;
	});

#if !TARGET_OS_OSX
	if (os_unlikely(!found && _dd_is_shared_ipad() && id == 501)) {
		filter.filterValue.id = _dd_get_foreground_uid();
		DarwinDirectoryRecordStoreApplyWithFilter(type, &filter, ^(darwin_directory_record_t record, bool *stop) {
			applier(record, stop);
		});
	}
#endif // !TARGET_OS_OSX
}

OS_ALWAYS_INLINE
static inline void
_dd_foreach_record_with_name(darwin_directory_type_t type, const char *name, _dd_record_applier_t applier)
{
	if (name == NULL) {
		return;
	}

	// "mobile" on embedded is a special name:
	//
	// * Prior to the comm page being initialized, it behaves normally.
	// * After the comm page has been initialized on shared iPad, lookups of
	//   "mobile" may be redirected to the foreground user.

	struct darwin_directory_filter_s filter = {
		.filterBy = DARWIN_DIRECTORY_FILTER_BY_NAME,
		.filterValue.name = name,
	};

	__block bool found = false;
	DarwinDirectoryRecordStoreApplyWithFilter(type, &filter, ^(darwin_directory_record_t record, bool *stop) {
		applier(record, stop);
		found = true;
	});

#if !TARGET_OS_OSX
	if (os_unlikely(!found && _dd_is_shared_ipad() && strcmp(name, "mobile") == 0)) {
		filter.filterBy = DARWIN_DIRECTORY_FILTER_BY_ID;
		filter.filterValue.id = _dd_get_foreground_uid();
		DarwinDirectoryRecordStoreApplyWithFilter(type, &filter, ^(darwin_directory_record_t record, bool *stop) {
			applier(record, stop);
		});
	}
#endif // !TARGET_OS_OSX
}

OS_ALWAYS_INLINE
static inline void
_dd_foreach_record_with_uuid(darwin_directory_type_t type, _Nonnull const uuid_t uuid, _dd_record_applier_t applier)
{
	struct darwin_directory_filter_s filter;
	filter.filterBy = DARWIN_DIRECTORY_FILTER_BY_UUID;
	uuid_copy(filter.filterValue.uuid, uuid);

	DarwinDirectoryRecordStoreApplyWithFilter(type, &filter, ^(darwin_directory_record_t record, bool *stop) {
		applier(record, stop);
	});
}

OS_ALWAYS_INLINE OS_WARN_RESULT
static inline bool
_dd_user_is_member_of_group(const char *username, darwin_directory_record_t group)
{
	// Check the list of member names to see if the user is a member.
	for (size_t i = 0; group->attributes.group.memberNames[i] != NULL; i++) {
		if (strcmp(group->attributes.group.memberNames[i], username) == 0) {
			return true;
		}
	}

	return false;
}

OS_ASSUME_NONNULL_END

#endif /* DARWIN_DIRECTORY_HELPERS_H */
