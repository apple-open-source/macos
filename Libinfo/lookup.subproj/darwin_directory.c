//
//  darwin_directory.c
//  Libinfo
//

#ifdef DARWIN_DIRECTORY_AVAILABLE

#include <DarwinDirectory/RecordStore_priv.h>
#include <os/cleanup.h>
#include <os/feature_private.h>
#include <si_data.h>
#include <si_module.h>
#include <sys/reason.h>

#include "darwin_directory_helpers.h"

#pragma mark - Users

static si_item_t *
_dd_extract_user(si_mod_t *si, darwin_directory_record_t user)
{
	struct passwd p = {
		.pw_name = (char *)user->name,
		.pw_passwd = "*",
		.pw_uid = user->id,
		.pw_gid = user->attributes.user.primaryGroupID,
		.pw_change = (time_t)0,
		.pw_expire = (time_t)0,
		.pw_class = "",
		.pw_gecos = (char *)user->attributes.user.fullName,
		.pw_dir = (char *)user->attributes.user.homeDirectory,
		.pw_shell = (char *)user->attributes.user.shell,
	};


	return (si_item_t *)LI_ils_create("L4488ss44LssssL", (unsigned long)si, CATEGORY_USER, 1, 0, 0,
									  p.pw_name, p.pw_passwd, p.pw_uid, p.pw_gid, p.pw_change,
									  p.pw_class, p.pw_gecos, p.pw_dir, p.pw_shell, p.pw_expire);
}

// getpwuid
static si_item_t *
darwin_directory_user_byuid(si_mod_t *si, uid_t uid)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_USERS, uid, ^(darwin_directory_record_t user, bool *stop) {
		item = _dd_extract_user(si, user);
		*stop = true;
	});

	return item;
}

// getpwnam
static si_item_t *
darwin_directory_user_byname(si_mod_t *si, const char *name)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_USERS, name, ^(darwin_directory_record_t user, bool *stop) {
		item = _dd_extract_user(si, user);
		*stop = true;
	});

	return item;
}

// getpwuuid
static si_item_t *
darwin_directory_user_byuuid(si_mod_t *si, uuid_t uuid)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_USERS, uuid, ^(darwin_directory_record_t user, bool *stop) {
		item = _dd_extract_user(si, user);
		*stop = true;
	});

	return item;
}

// getpwent
static si_list_t *
darwin_directory_user_all(si_mod_t *si)
{
	__block si_list_t *list = NULL;

	_dd_foreach_record(DARWIN_DIRECTORY_TYPE_USERS, ^(darwin_directory_record_t user, __unused bool *stop) {
		si_item_t *item = _dd_extract_user(si, user);
		if (item != NULL) {
			list = si_list_add(list, item);
			si_item_release(item);
		}
	});

	return list;
}

#pragma mark - Groups

static si_item_t *
_dd_extract_group(si_mod_t *si, darwin_directory_record_t group)
{
	struct group g = {
		.gr_gid = group->id,
		.gr_name = (char *)group->name,
		.gr_passwd = "*",
		.gr_mem = (char **)group->attributes.group.memberNames,
	};

	si_item_t *item = (si_item_t *)LI_ils_create("L4488ss4*", (unsigned long)si, CATEGORY_GROUP, 1, 0, 0,
												 g.gr_name, g.gr_passwd, g.gr_gid, g.gr_mem);

	return item;
}

// getgrgid
static si_item_t *
darwin_directory_group_bygid(struct si_mod_s *si, gid_t gid)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_GROUPS, gid, ^(darwin_directory_record_t group, bool *stop) {
		item = _dd_extract_group(si, group);
		*stop = true;
	});

	return item;
}

// getgrnam
static si_item_t *
darwin_directory_group_byname(struct si_mod_s *si, const char *name)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_GROUPS, name, ^(darwin_directory_record_t group, bool *stop) {
		item = _dd_extract_group(si, group);
		*stop = true;
	});

	return item;
}

// getgruuid
static si_item_t *
darwin_directory_group_byuuid(si_mod_t *si, uuid_t uuid)
{
	__block si_item_t *item = NULL;

	_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_GROUPS, uuid, ^(darwin_directory_record_t group, bool *stop) {
		item = _dd_extract_group(si, group);
		*stop = true;
	});

	return item;
}

// getgrent
static si_list_t *
darwin_directory_group_all(si_mod_t *si)
{
	__block si_list_t *list = NULL;

	_dd_foreach_record(DARWIN_DIRECTORY_TYPE_GROUPS, ^(darwin_directory_record_t group, __unused bool *stop) {
		si_item_t *item = _dd_extract_group(si, group);
		if (item != NULL) {
			list = si_list_add(list, item);
			si_item_release(item);
		}
	});

	return list;
}

static bool
_dd_user_is_member_of_group(const char *userName, darwin_directory_record_t group)
{
	// Check the list of member names to see if the user is a member.

	__block bool isMember = false;
	for (size_t i = 0; group->attributes.group.memberNames[i] != NULL; i++) {
		if (strcmp(group->attributes.group.memberNames[i], userName) == 0) {
			isMember = true;
			break;
		}
	}

	return isMember;
}

// get_grouplist
static si_item_t *
darwin_directory_grouplist(struct si_mod_s *si, const char *userName, __unused uint32_t maxGroups)
{
	//
	// 1. Find the user record's primary group ID.
	//

	__block bool found = false;
	__block gid_t primaryGID = 0;

	_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_USERS, userName, ^(darwin_directory_record_t user, bool *stop) {
		primaryGID = user->attributes.user.primaryGroupID;
		found = true;
		*stop = true;
	});

	// User wasn't found in Darwin Directory.
	if (!found) {
		return NULL;
	}

	__block size_t gidCount = 0;
	__block size_t gidListMax = 16;
	gid_t __block __os_free *gidList = malloc(gidListMax * sizeof(gid_t));
	if (os_unlikely(gidList == NULL)) {
		_dd_fatal_error("Failed to allocate memory for the group list");
	}

	//
	// 2. Check the group membership of every group record.
	//

	_dd_foreach_record(DARWIN_DIRECTORY_TYPE_GROUPS, ^(darwin_directory_record_t group, __unused bool *stop) {
		if (!_dd_user_is_member_of_group(userName, group)) {
			return;
		}

		gid_t gid = group->id;

		if (gidCount == gidListMax) {
			gidListMax *= 2;
			gidList = reallocf(gidList, gidListMax * sizeof(gid_t));
			if (os_unlikely(gidList == NULL)) {
				_dd_fatal_error("Failed to re-allocate memory for the group list");
			}
		}

		gidList[gidCount++] = gid;
	});

	//
	// 3. Return the gid list to libinfo.
	//

	if (gidCount == 0) {
		return NULL;
	}

	return (si_item_t *)LI_ils_create("L4488s4@", (unsigned long)si, CATEGORY_GROUPLIST,
									  1, 0, 0, userName, gidCount, gidCount * sizeof(gid_t), gidList);
}

#pragma mark -

si_mod_t *
si_module_static_darwin_directory(void)
{
	static struct si_mod_vtable_s darwin_directory_vtable = {
		.sim_user_byname = &darwin_directory_user_byname,
		.sim_user_byuid = &darwin_directory_user_byuid,
		.sim_user_byuuid = &darwin_directory_user_byuuid,
		.sim_user_all = &darwin_directory_user_all,

		.sim_group_byname = &darwin_directory_group_byname,
		.sim_group_bygid = &darwin_directory_group_bygid,
		.sim_group_byuuid = &darwin_directory_group_byuuid,
		.sim_group_all = &darwin_directory_group_all,
		.sim_grouplist = &darwin_directory_grouplist,
	};

	// If the feature flag isn't set, NULL the vtable so this module is skipped
	// for all lookups.  Once the module is built into the libinfo search list
	// it must return a non-NULL si_mod_t * to avoid contaminating errno with a
	// failed attempt to load the module from disk.
	if (!os_feature_enabled_simple(DarwinDirectory, LibinfoLookups, false)) {
		memset(&darwin_directory_vtable, 0, sizeof(darwin_directory_vtable));
	}

	static si_mod_t si = {
		.vers = 1,
		.refcount = 1,
		.flags = SI_MOD_FLAG_STATIC,

		.private = NULL,
		.vtable = &darwin_directory_vtable,
	};

	static dispatch_once_t once;
	dispatch_once(&once, ^{
		si.name = strdup("darwin_directory");
	});

	return &si;
}

#endif // DARWIN_DIRECTORY_AVAILABLE
