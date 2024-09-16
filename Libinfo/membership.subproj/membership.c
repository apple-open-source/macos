/*
 * Copyright (c) 2004-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "libinfo_common.h"
#include "od_debug.h"

#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <mach/mach.h>
#include "membership.h"
#include "membershipPriv.h"
#include <servers/bootstrap.h>
#include <libkern/OSByteOrder.h>
#ifdef DS_AVAILABLE
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <os/activity.h>
#include <opendirectory/odipc.h>
#include <pthread.h>
#include <mach-o/dyld_priv.h>
#endif
#ifdef DARWIN_DIRECTORY_AVAILABLE
#include "darwin_directory_enabled.h"
#include "darwin_directory_helpers.h"
#include <os/cleanup.h>
#endif // DARWIN_DIRECTORY_AVAIABLE

static const uuid_t _user_compat_prefix = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};
static const uuid_t _group_compat_prefix = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0x00, 0x00, 0x00, 0x00};

#define COMPAT_PREFIX_LEN	(sizeof(uuid_t) - sizeof(id_t))

#if DS_AVAILABLE
#define MBR_OS_ACTIVITY(_desc) \
	os_activity_t activity __attribute__((__cleanup__(_mbr_auto_os_release))) = os_activity_create(_desc, OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT); \
	os_activity_scope(activity)
#else
#define MBR_OS_ACTIVITY(_desc)
#endif

#ifdef DS_AVAILABLE

int _si_opendirectory_disabled;
static xpc_pipe_t __mbr_pipe; /* use accessor */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
__private_extern__ xpc_object_t _od_rpc_call(const char *procname, xpc_object_t payload, xpc_pipe_t (*get_pipe)(bool));

#endif

#ifdef DS_AVAILABLE
static void
_mbr_fork_child(void)
{
	if (__mbr_pipe != NULL) {
		xpc_pipe_invalidate(__mbr_pipe);
		/* disable release due to 10649340, it will cause a minor leak for each fork without exec */
		// xpc_release(__mbr_pipe);
		__mbr_pipe = NULL;
	}
	
	pthread_mutex_unlock(&mutex);
}
#endif

#ifdef DS_AVAILABLE
static void
_mbr_fork_prepare(void)
{
	pthread_mutex_lock(&mutex);
}
#endif

#ifdef DS_AVAILABLE
static void
_mbr_fork_parent(void)
{
	pthread_mutex_unlock(&mutex);
}
#endif

#ifdef DS_AVAILABLE
static void
_mbr_auto_os_release(os_activity_t *activity)
{
	os_release(*activity);
	(*activity) = NULL;
}

XPC_RETURNS_RETAINED
static xpc_pipe_t
_mbr_xpc_pipe(bool resetPipe)
{
	static dispatch_once_t once;
	xpc_pipe_t pipe = NULL;
	
	dispatch_once(&once, ^(void) {
		char *xbs_disable;
		
		/* if this is a build environment we ignore opendirectoryd */
		xbs_disable = getenv("XBS_DISABLE_LIBINFO");
		if (xbs_disable != NULL && strcmp(xbs_disable, "YES") == 0) {
			_si_opendirectory_disabled = 1;
			return;
		}
		
		pthread_atfork(_mbr_fork_prepare, _mbr_fork_parent, _mbr_fork_child);
	});
	
	if (_si_opendirectory_disabled == 1) {
		return NULL;
	}
	
	pthread_mutex_lock(&mutex);
	if (resetPipe) {
		xpc_release(__mbr_pipe);
		__mbr_pipe = NULL;
	}
	
	if (__mbr_pipe == NULL) {
		if (!dyld_process_is_restricted() && od_debug_enabled()) {
			__mbr_pipe = xpc_pipe_create(kODMachMembershipPortNameDebug, 0);
		} else {
			__mbr_pipe = xpc_pipe_create(kODMachMembershipPortName, XPC_PIPE_PRIVILEGED | XPC_PIPE_PROPAGATE_QOS);
		}
	}
	
	if (__mbr_pipe != NULL) pipe = xpc_retain(__mbr_pipe);
	pthread_mutex_unlock(&mutex);
	
	return pipe;
}

static void
_mbr_xpc_pipe_close()
{
	pthread_mutex_lock(&mutex);
	if (__mbr_pipe != NULL) {
		xpc_release(__mbr_pipe);
		__mbr_pipe = NULL;
	}
	pthread_mutex_unlock(&mutex);
}
#endif

static bool
_mbr_od_available(void)
{
#if DS_AVAILABLE
	xpc_pipe_t pipe = _mbr_xpc_pipe(false);
	if (pipe != NULL) {
		xpc_release(pipe);
		return true;
	}
#endif
	return false;
}

static bool
parse_compatibility_uuid(const uuid_t uu, id_t *result, int *rec_type)
{
	id_t tempID;

	if (memcmp(uu, _user_compat_prefix, COMPAT_PREFIX_LEN) == 0) {
		memcpy(&tempID, &uu[COMPAT_PREFIX_LEN], sizeof(tempID));
		(*result) = ntohl(tempID);
		if (rec_type != NULL) {
			(*rec_type) = MBR_REC_TYPE_USER;
		}
		return true;
	} else if (memcmp(uu, _group_compat_prefix, COMPAT_PREFIX_LEN) == 0) {
		memcpy(&tempID, &uu[COMPAT_PREFIX_LEN], sizeof(tempID));
		(*result) = ntohl(tempID);
		if (rec_type != NULL) {
			(*rec_type) = MBR_REC_TYPE_GROUP;
		}
		return true;
	}
	return false;
}

#if !DS_AVAILABLE
static bool
compatibility_name_for_id(id_t id, int rec_type, char **result)
{
	int bufsize;

	if ((bufsize = sysconf(_SC_GETPW_R_SIZE_MAX)) == -1)
		return false;

	if (rec_type == MBR_REC_TYPE_USER) {
		char buffer[bufsize];
		struct passwd pwd, *pwdp = NULL;

		if (getpwuid_r(id, &pwd, buffer, bufsize, &pwdp) != 0 || pwdp == NULL) {
			return false;
		}
		(*result) = strdup(pwd.pw_name);
		return (*result) != NULL;
	} else if (rec_type == MBR_REC_TYPE_GROUP) {
		char buffer[bufsize];
		struct group grp, *grpp = NULL;

		if (getgrgid_r(id, &grp, buffer, bufsize, &grpp) != 0 || grpp == NULL) {
			return false;
		}
		(*result) = strdup(grp.gr_name);
		return (*result) != NULL;
	}
	return false;
}

static bool
compatibility_name_for_uuid(const uuid_t uu, char **result, int *rec_type)
{
	int temp_type;
	id_t id;

	if (parse_compatibility_uuid(uu, &id, &temp_type) &&
	    compatibility_name_for_id(id, temp_type, result)) {
		if (rec_type != NULL) {
			(*rec_type) = temp_type;
		}
		return true;
	} else {
		return false;
	}
}
#endif

#ifdef DARWIN_DIRECTORY_AVAILABLE
OS_MALLOC
static void *
_dd_extract_result(int target_type, darwin_directory_record_t record)
{
	id_t *tempID = NULL;
	uint8_t *tempUUID = NULL;

	// Ensure the record type matches the desired target type.
	if ((target_type == ID_TYPE_UID || target_type == ID_TYPE_USERNAME) &&
			record->type != DARWIN_DIRECTORY_TYPE_USERS) {
		return NULL;
	} else if ((target_type == ID_TYPE_GID || target_type == ID_TYPE_GROUPNAME) &&
			   record->type != DARWIN_DIRECTORY_TYPE_GROUPS) {
		return NULL;
	}

	switch (target_type) {
		case ID_TYPE_UID:
		case ID_TYPE_GID:
		case ID_TYPE_UID_OR_GID:
			tempID = malloc(sizeof(id_t));
			if (os_unlikely(tempID == NULL)) {
				_dd_fatal_error("Failed to allocate memory for a uid/gid");
			}
			*tempID = record->id;
			return tempID;

		case ID_TYPE_USERNAME:
		case ID_TYPE_GROUPNAME:
		case ID_TYPE_NAME:
			return strdup(record->name);

		case ID_TYPE_UUID:
			tempUUID = malloc(sizeof(uuid_t));
			if (os_unlikely(tempUUID == NULL)) {
				_dd_fatal_error("Failed to allocate memory for a uuid");
			}
			uuid_copy(tempUUID, record->uuid);
			return tempUUID;

		default:
			return NULL;
	}
}

static bool
_dd_target_type_is_valid_for_id_type(int id_type, int target_type)
{
	switch (target_type) {
		case ID_TYPE_UID:
			return id_type == ID_TYPE_USERNAME || id_type == ID_TYPE_UUID;
		case ID_TYPE_GID:
			return id_type == ID_TYPE_GROUPNAME || id_type == ID_TYPE_UUID;
		case ID_TYPE_USERNAME:
			return id_type == ID_TYPE_UID || id_type == ID_TYPE_UUID;
		case ID_TYPE_GROUPNAME:
			return id_type == ID_TYPE_GID || id_type == ID_TYPE_UUID;
		case ID_TYPE_UUID:
			return true; // Any id_type is valid when asking for a UUID
		case ID_TYPE_UID_OR_GID:
			return id_type == ID_TYPE_USERNAME || id_type == ID_TYPE_GROUPNAME || id_type == ID_TYPE_UUID;
		case ID_TYPE_NAME:
			return id_type == ID_TYPE_UID || id_type == ID_TYPE_GID || id_type == ID_TYPE_UUID;
		default:
			return false;
	}
}

static int
_dd_mbr_identifier_translate(int id_type, const void *identifier, size_t identifier_size, int target_type, void **result, int *rec_type)
{
	__block int local_rec_type = -1;
	__block int rc = ENOENT;

	if (!_dd_target_type_is_valid_for_id_type(id_type, target_type)) {
		return EINVAL;
	}

	switch (id_type) {
		case ID_TYPE_UID:
			_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_USERS, *(id_t*)identifier, ^(darwin_directory_record_t user, bool *stop) {
				*result = _dd_extract_result(target_type, user);
				local_rec_type = MBR_REC_TYPE_USER;
				rc = 0;
				*stop = true;
			});

			// The man page says that translating from uid to uuid will create
			// a fake uuid when the uid isn't found.
			if (os_unlikely(*result == NULL) && target_type == ID_TYPE_UUID) {
				uint8_t *tempUUID = malloc(sizeof(uuid_t));
				if (os_unlikely(tempUUID == NULL)) {
					_dd_fatal_error("Failed to allocate memory for a compatibility user uuid");
				}
				uuid_copy(tempUUID, _user_compat_prefix);
				*((id_t*)&tempUUID[COMPAT_PREFIX_LEN]) = htonl(*(id_t*)identifier);
				*result = tempUUID;
				local_rec_type = MBR_REC_TYPE_USER;
				rc = 0;
			}
			break;

		case ID_TYPE_GID:
			_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_GROUPS, *(id_t*)identifier, ^(darwin_directory_record_t group, bool *stop) {
				*result = _dd_extract_result(target_type, group);
				local_rec_type = MBR_REC_TYPE_GROUP;
				rc = 0;
				*stop = true;
			});

			// The man page says that translating from gid to uuid will create
			// a fake uuid when the gid isn't found.
			if (os_unlikely(*result == NULL) && target_type == ID_TYPE_UUID) {
				uint8_t *tempUUID = malloc(sizeof(uuid_t));
				if (os_unlikely(tempUUID == NULL)) {
					_dd_fatal_error("Failed to allocate memory for a compatibility group uuid");
				}
				uuid_copy(tempUUID, _group_compat_prefix);
				*((id_t*)&tempUUID[COMPAT_PREFIX_LEN]) = htonl(*(id_t*)identifier);
				*result = tempUUID;
				local_rec_type = MBR_REC_TYPE_GROUP;
				rc = 0;
			}
			break;

		case ID_TYPE_USERNAME:
			_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_USERS, identifier, ^(darwin_directory_record_t user, bool *stop) {
				*result = _dd_extract_result(target_type, user);
				local_rec_type = MBR_REC_TYPE_USER;
				rc = 0;
				*stop = true;
			});
			break;

		case ID_TYPE_GROUPNAME:
			_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_GROUPS, identifier, ^(darwin_directory_record_t group, bool *stop) {
				*result = _dd_extract_result(target_type, group);
				local_rec_type = MBR_REC_TYPE_GROUP;
				rc = 0;
				*stop = true;
			});
			break;

		case ID_TYPE_UUID:
			_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_USERS, identifier, ^(darwin_directory_record_t user, bool *stop) {
				*result = _dd_extract_result(target_type, user);
				local_rec_type = MBR_REC_TYPE_USER;
				rc = 0;
				*stop = true;
			});

			if (*result == NULL) {
				_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_GROUPS, identifier, ^(darwin_directory_record_t group, bool *stop) {
					*result = _dd_extract_result(target_type, group);
					local_rec_type = MBR_REC_TYPE_GROUP;
					rc = 0;
					*stop = true;
				});
			}

			// The man page says that translating from uuid to uid/gid will
			// return a made-up uid/gid when the uuid isn't found.  That's only
			// partially true: the uuid has to be one of the well-known prefixes.
			if (os_unlikely(*result == NULL) && (target_type == ID_TYPE_UID || target_type == ID_TYPE_GID || target_type == ID_TYPE_UID_OR_GID)) {
				id_t *tempID = malloc(sizeof(*tempID));
				if (os_unlikely(tempID == NULL)) {
					_dd_fatal_error("Failed to allocate memory for a compatibility uid/gid");
				}
				if (parse_compatibility_uuid(identifier, tempID, rec_type)) {
					*result = tempID;
					rc = 0;
				} else {
					free(tempID);
				}
			}

			break;

		default:
			rc = EINVAL;
			break;
	}

	if (rec_type != NULL && *result != NULL) {
		*rec_type = local_rec_type;
	}

	return rc;
}
#endif // DARWIN_DIRECTORY_AVAILABLE

LIBINFO_EXPORT
int
mbr_identifier_translate(int id_type, const void *identifier, size_t identifier_size, int target_type, void **result, int *rec_type)
{
#if DS_AVAILABLE
	xpc_object_t payload, reply;
#endif
	id_t tempID;
	size_t identifier_len;
	int rc = EIO;
	
	if (identifier == NULL || result == NULL || identifier_size == 0) return EIO;

	if (identifier_size == -1) {
		identifier_size = strlen(identifier);
	} else {
		/* 10898647: For types that are known to be strings, send the smallest necessary amount of data. */
		switch (id_type) {
		case ID_TYPE_USERNAME:
		case ID_TYPE_GROUPNAME:
		case ID_TYPE_GROUP_NFS:
		case ID_TYPE_USER_NFS:
		case ID_TYPE_X509_DN:
		case ID_TYPE_KERBEROS:
		case ID_TYPE_NAME:
			identifier_len = strlen(identifier);
			if (identifier_size > identifier_len) {
				identifier_size = identifier_len;
			}
			break;
		/* ensure the fixed length types have correct sizes */
		case ID_TYPE_UID:
		case ID_TYPE_GID:
			if (identifier_size != sizeof(id_t)) {
				return EINVAL;
			}
			break;
		case ID_TYPE_UUID:
			if (identifier_size != sizeof(uuid_t)) {
				return EINVAL;
			}
			break;
		}
	}

#ifdef DARWIN_DIRECTORY_AVAILABLE
	if (_darwin_directory_enabled()) {
		rc = _dd_mbr_identifier_translate(id_type, identifier, identifier_size, target_type, result, rec_type);

		// If Darwin Directory didn't find the identifier (ENOENT), fall through
		// to the default code or OD (if OD is available).  For all other cases
		// Darwin Directory's result is authoritative since it found the identifier.
		if (rc != ENOENT) {
			return rc;
		}
	}
#endif // DARWIN_DIRECTORY_AVAILABLE

	switch (target_type) {
		case ID_TYPE_GID:
		case ID_TYPE_UID:
		case ID_TYPE_UID_OR_GID:
			/* shortcut UUIDs using compatibility prefixes */
			if (id_type == ID_TYPE_UUID) {
				id_t *tempRes;

				tempRes = malloc(sizeof(*tempRes));
				if (tempRes == NULL) return ENOMEM;

				if (parse_compatibility_uuid(identifier, tempRes, rec_type)) {
					(*result) = tempRes;
					return 0;
				}
				free(tempRes);
			}
			break;
			
		case ID_TYPE_UUID:
			/* if this is a UID or GID translation, we shortcut UID/GID 0 */
			/* or if no OD, we return compatibility UUIDs */
			switch (id_type) {
				case ID_TYPE_UID:
					tempID = *((id_t *) identifier);
					if ((tempID == 0) || (_mbr_od_available() == false)) {
						uint8_t *tempUU = malloc(sizeof(uuid_t));
						if (tempUU == NULL) return ENOMEM;
						uuid_copy(tempUU, _user_compat_prefix);
						*((id_t *) &tempUU[COMPAT_PREFIX_LEN]) = htonl(tempID);
						(*result) = tempUU;
						if (rec_type != NULL) {
							(*rec_type) = MBR_REC_TYPE_USER;
						}
						return 0;
					}
					break;
					
				case ID_TYPE_GID:
					tempID = *((id_t *) identifier);
					if ((tempID == 0) || (_mbr_od_available() == false)) {
						uint8_t *tempUU = malloc(sizeof(uuid_t));
						if (tempUU == NULL) return ENOMEM;
						uuid_copy(tempUU, _group_compat_prefix);
						*((id_t *) &tempUU[COMPAT_PREFIX_LEN]) = htonl(tempID);
						(*result) = tempUU;
						if (rec_type != NULL) {
							(*rec_type) = MBR_REC_TYPE_GROUP;
						}
						return 0;
					}
					break;
			}
			break;

		case ID_TYPE_USERNAME:
		case ID_TYPE_GROUPNAME:
		case ID_TYPE_NAME:
#if !DS_AVAILABLE
			/* Convert compatibility UUIDs to names in-process. */
			if (id_type == ID_TYPE_UUID) {
				if (compatibility_name_for_uuid(identifier, (char **)result, rec_type)) {
					return 0;
				}
			} else if (id_type == ID_TYPE_UID) {
				tempID = *((id_t *) identifier);
				if (compatibility_name_for_id(tempID, MBR_REC_TYPE_USER, (char **)result)) {
					if (rec_type != NULL) {
						(*rec_type) = MBR_REC_TYPE_USER;
					}
					return 0;
				}
			} else if (id_type == ID_TYPE_GID) {
				tempID = *((id_t *) identifier);
				if (compatibility_name_for_id(tempID, MBR_REC_TYPE_GROUP, (char **)result)) {
					if (rec_type != NULL) {
						(*rec_type) = MBR_REC_TYPE_GROUP;
					}
					return 0;
				}
			}
#endif
			break;
	}

#if DS_AVAILABLE
	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (payload == NULL) return EIO;

	MBR_OS_ACTIVITY("Membership API: translate identifier");

	xpc_dictionary_set_int64(payload, "requesting", target_type);
	xpc_dictionary_set_int64(payload, "type", id_type);
	xpc_dictionary_set_data(payload, "identifier", identifier, identifier_size);
	
	reply = _od_rpc_call("mbr_identifier_translate", payload, _mbr_xpc_pipe);
	if (reply != NULL) {
		const void *reply_id;
		size_t idLen;
		
		rc = (int) xpc_dictionary_get_int64(reply, "error");
		if (rc == 0) {
			reply_id = xpc_dictionary_get_data(reply, "identifier", &idLen);
			if (reply_id != NULL) {
				char *identifier = malloc(idLen);
				if (identifier == NULL) return ENOMEM;

				memcpy(identifier, reply_id, idLen); // should already be NULL terminated, etc.
				(*result) = identifier;
				
				if (rec_type != NULL) {
					(*rec_type) = (int) xpc_dictionary_get_int64(reply, "rectype");
				}
			} else {
				(*result) = NULL;
				rc = ENOENT;
			}
		}
		
		xpc_release(reply);
	}
	
	xpc_release(payload);
#endif
	
	return rc;
}

LIBINFO_EXPORT
int
mbr_uid_to_uuid(uid_t id, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_UID, &id, sizeof(id), uu);
}

LIBINFO_EXPORT
int
mbr_gid_to_uuid(gid_t id, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_GID, &id, sizeof(id), uu);
}

LIBINFO_EXPORT
int
mbr_uuid_to_id(const uuid_t uu, uid_t *id, int *id_type)
{
	id_t *result;
	int local_type;
	int rc;
	
	rc = mbr_identifier_translate(ID_TYPE_UUID, uu, sizeof(uuid_t), ID_TYPE_UID_OR_GID, (void **) &result, &local_type);
	if (rc == 0) {
		switch (local_type) {
			case MBR_REC_TYPE_GROUP:
				(*id_type) = ID_TYPE_GID;
				break;
				
			case MBR_REC_TYPE_USER:
				(*id_type) = ID_TYPE_UID;
				break;
				
			default:
				(*id_type) = -1;
				break;
		}
		
		(*id) = (*result);
		free(result);
	}
	
	return rc;
}

LIBINFO_EXPORT
int
mbr_sid_to_uuid(const nt_sid_t *sid, uuid_t uu)
{
#ifdef DS_AVAILABLE
	return mbr_identifier_to_uuid(ID_TYPE_SID, sid, sizeof(*sid), uu);
#else
	return EIO;
#endif
}

LIBINFO_EXPORT
int
mbr_identifier_to_uuid(int id_type, const void *identifier, size_t identifier_size, uuid_t uu)
{
	uint8_t *result;
	int rc;
	
	rc = mbr_identifier_translate(id_type, identifier, identifier_size, ID_TYPE_UUID, (void **) &result, NULL);
	if (rc == 0) {
		uuid_copy(uu, result);
		free(result);
	}
	else if ((rc == EIO) && (_mbr_od_available() == false)) {
		switch (id_type) {
			case ID_TYPE_USERNAME:
			{
				struct passwd *pw = getpwnam(identifier);
				if (pw) {
					rc = mbr_identifier_translate(ID_TYPE_UID, &(pw->pw_uid), sizeof(id_t), ID_TYPE_UUID, (void **) &result, NULL);
					if (rc == 0) {
						uuid_copy(uu, result);
						free(result);
					}
				}
				break;
			}
			case ID_TYPE_GROUPNAME:
			{
				struct group *grp = getgrnam(identifier);
				if (grp) {
					rc = mbr_identifier_translate(ID_TYPE_GID, &(grp->gr_gid), sizeof(id_t), ID_TYPE_UUID, (void **) &result, NULL);
					if (rc == 0) {
						uuid_copy(uu, result);
						free(result);
					}
				}
				break;
			}

			default:
				break;
		}
	}
	
	return rc;
}

LIBINFO_EXPORT
int
mbr_uuid_to_sid_type(const uuid_t uu, nt_sid_t *sid, int *id_type)
{
#ifdef DS_AVAILABLE
	void *result;
	int local_type;
	int rc;
	
	rc = mbr_identifier_translate(ID_TYPE_UUID, uu, sizeof(uuid_t), ID_TYPE_SID, &result, &local_type);
	if (rc == 0) {
		memcpy(sid, result, sizeof(nt_sid_t));
		if (id_type != NULL) {
			/* remap ID types */
			switch (local_type) {
				case MBR_REC_TYPE_USER:
					(*id_type) = SID_TYPE_USER;
					break;

				case MBR_REC_TYPE_GROUP:
					(*id_type) = SID_TYPE_GROUP;
					break;

				default:
					break;
			}
		}
		
		free(result);
	}
	
	return rc;
#else
	return EIO;
#endif
}

LIBINFO_EXPORT
int
mbr_uuid_to_sid(const uuid_t uu, nt_sid_t *sid)
{
#ifdef DS_AVAILABLE
	int type, status;

	type = 0;

	status = mbr_uuid_to_sid_type(uu, sid, &type);
	if (status != 0) return status;

	return 0;
#else
	return EIO;
#endif
}

LIBINFO_EXPORT
int
mbr_check_membership(const uuid_t user, const uuid_t group, int *ismember)
{
	return mbr_check_membership_ext(ID_TYPE_UUID, user, sizeof(uuid_t), ID_TYPE_UUID, group, 0, ismember);
}

LIBINFO_EXPORT
int
mbr_check_membership_refresh(const uuid_t user, uuid_t group, int *ismember)
{
	return mbr_check_membership_ext(ID_TYPE_UUID, user, sizeof(uuid_t), ID_TYPE_UUID, group, 1, ismember);
}

#ifdef DS_AVAILABLE
static int
_od_mbr_check_membership_ext(int userid_type, const void *userid, size_t userid_size, int groupid_type, const void *groupid, int refresh, int *isMember)
{
	xpc_object_t payload, reply;
	int rc = 0;

	MBR_OS_ACTIVITY("Membership API: Validating user is a member of group");
	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (payload == NULL) return ENOMEM;

	xpc_dictionary_set_int64(payload, "user_idtype", userid_type);
	xpc_dictionary_set_data(payload, "user_id", userid, userid_size);
	xpc_dictionary_set_int64(payload, "group_idtype", groupid_type);
	xpc_dictionary_set_bool(payload, "refresh", refresh);
	
	switch (groupid_type) {
		case ID_TYPE_GROUPNAME:
		case ID_TYPE_GROUP_NFS:
			xpc_dictionary_set_data(payload, "group_id", groupid, strlen(groupid));
			break;
			
		case ID_TYPE_GID:
			xpc_dictionary_set_data(payload, "group_id", groupid, sizeof(id_t));
			break;
			
		case ID_TYPE_SID:
			xpc_dictionary_set_data(payload, "group_id", groupid, sizeof(nt_sid_t));
			break;
			
		case ID_TYPE_UUID:
			xpc_dictionary_set_data(payload, "group_id", groupid, sizeof(uuid_t));
			break;
			
		default:
			rc = EINVAL;
			break;
	}
	
	if (rc == 0) {
		reply = _od_rpc_call("mbr_check_membership", payload, _mbr_xpc_pipe);
		if (reply != NULL) {
			rc = (int) xpc_dictionary_get_int64(reply, "error");
			(*isMember) = xpc_dictionary_get_bool(reply, "ismember");
			xpc_release(reply);
		} else {
			rc = EIO;
		}
	}
	
	xpc_release(payload);
	
	return rc;
}
#endif // DS_AVAILABLE

#ifdef DARWIN_DIRECTORY_AVAILABLE
OS_ENUM(dd_mbr_check_result, int,
	DD_MBR_SUCCESS = 0,
	DD_MBR_USER_NOT_FOUND,
	DD_MBR_GROUP_NOT_FOUND,
	DD_MBR_INVALID_PARAMETER,
);

OS_ALWAYS_INLINE OS_WARN_RESULT
static inline bool
_dd_user_belongs_to_group(const char *username, gid_t userPrimaryGroupID, darwin_directory_record_t group)
{
	return userPrimaryGroupID == group->id || _dd_user_is_member_of_group(username, group);
}

OS_WARN_RESULT
static dd_mbr_check_result_t
_dd_mbr_check_membership_ext(int userid_type, const void *userid, size_t userid_size, int groupid_type, const void *groupid, __unused int refresh, int *isMember)
{
	*isMember = 0;

	if (userid == NULL || groupid == NULL) {
		return DD_MBR_INVALID_PARAMETER;
	}

	id_t tempID = -2;
	__block char __os_free *username = NULL;
	__block gid_t userPrimaryGroupID = -2;

	switch (userid_type) {
		case ID_TYPE_USERNAME:
			// Yes we already have the username but we need to make sure the user
			// exists in the record store.
			_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_USERS, userid, ^(darwin_directory_record_t user, bool *stop) {
				username = strdup(user->name);
				userPrimaryGroupID = user->attributes.user.primaryGroupID;
				*stop = true;
			});
			break;

		case ID_TYPE_UUID:
			if (userid_size != sizeof(uuid_t)) {
				return DD_MBR_INVALID_PARAMETER;
			}

			_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_USERS, userid, ^(darwin_directory_record_t user, bool *stop) {
				username = strdup(user->name);
				userPrimaryGroupID = user->attributes.user.primaryGroupID;
				*stop = true;
			});
			break;

		case ID_TYPE_UID:
			if (userid_size != sizeof(id_t)) {
				return DD_MBR_INVALID_PARAMETER;
			}

			tempID = *((id_t *)userid);
			_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_USERS, tempID, ^(darwin_directory_record_t user, bool *stop) {
				username = strdup(user->name);
				userPrimaryGroupID = user->attributes.user.primaryGroupID;
				*stop = true;
			});
			break;

		default:
			return DD_MBR_INVALID_PARAMETER;
	}

	if (username == NULL) {
		return DD_MBR_USER_NOT_FOUND;
	}

	__block bool groupFound = false;
	switch (groupid_type) {
		case ID_TYPE_GROUPNAME:
			_dd_foreach_record_with_name(DARWIN_DIRECTORY_TYPE_GROUPS, groupid, ^(darwin_directory_record_t group, bool *stop) {
				*isMember = (int)_dd_user_belongs_to_group(username, userPrimaryGroupID, group);
				groupFound = true;
				*stop = true;
			});
			break;

		case ID_TYPE_UUID:
			_dd_foreach_record_with_uuid(DARWIN_DIRECTORY_TYPE_GROUPS, groupid, ^(darwin_directory_record_t group, bool *stop) {
				*isMember = (int)_dd_user_belongs_to_group(username, userPrimaryGroupID, group);
				groupFound = true;
				*stop = true;
			});
			break;

		case ID_TYPE_GID:
			tempID = *((id_t *)groupid);
			_dd_foreach_record_with_id(DARWIN_DIRECTORY_TYPE_GROUPS, tempID, ^(darwin_directory_record_t group, bool *stop) {
				*isMember = (int)_dd_user_belongs_to_group(username, userPrimaryGroupID, group);
				groupFound = true;
				*stop = true;
			});
			break;

		default:
			return DD_MBR_INVALID_PARAMETER;
	}

	if (!groupFound) {
		return DD_MBR_GROUP_NOT_FOUND;
	}

	return DD_MBR_SUCCESS;
}
#endif // DARWIN_DIRECTORY_AVAILABLE

LIBINFO_EXPORT
int
mbr_check_membership_ext(int userid_type, const void *userid, size_t userid_size, int groupid_type, const void *groupid, int refresh, int *isMember)
{
	int result = EIO;

#ifdef DARWIN_DIRECTORY_AVAILABLE
	if (_darwin_directory_enabled()) {
		result = _dd_mbr_check_membership_ext(userid_type, userid, userid_size, groupid_type, groupid, refresh, isMember);
		switch (result) {
			case DD_MBR_SUCCESS:
				return 0;

			case DD_MBR_USER_NOT_FOUND:
				// The user wasn't found in Darwin Directory.  Let OD handle it
				// in case this is a network user.
				break;

			case DD_MBR_GROUP_NOT_FOUND:
				// User was found but the group wasn't.  According to the man
				// page this is not an error.  The user simply isn't a member
				// of a non-existent group.
				return 0;

			case DD_MBR_INVALID_PARAMETER:
			default:
				return EINVAL;
		}
	}
#endif

#ifdef DS_AVAILABLE
	result = _od_mbr_check_membership_ext(userid_type, userid, userid_size, groupid_type, groupid, refresh, isMember);
#endif

	return result;
}

LIBINFO_EXPORT
int
mbr_check_membership_by_id(uuid_t user, gid_t group, int *ismember)
{
	return mbr_check_membership_ext(ID_TYPE_UUID, user, sizeof(uuid_t), ID_TYPE_GID, &group, 0, ismember);
}

LIBINFO_EXPORT
int
mbr_reset_cache()
{
#ifdef DS_AVAILABLE
	MBR_OS_ACTIVITY("Membership API: Flush the membership cache");
	xpc_object_t result = _od_rpc_call("mbr_cache_flush", NULL, _mbr_xpc_pipe);
	if (result) {
		xpc_release(result);
	}
	return 0;
#else
	return EIO;
#endif
}

LIBINFO_EXPORT
int
mbr_close_connections()
{
#ifdef DS_AVAILABLE
	_mbr_xpc_pipe_close();
	return 0;
#else
	return EIO;
#endif
}


LIBINFO_EXPORT
int
mbr_user_name_to_uuid(const char *name, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_USERNAME, name, -1, uu);
}

LIBINFO_EXPORT
int
mbr_group_name_to_uuid(const char *name, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_GROUPNAME, name, -1, uu);
}

#ifdef DS_AVAILABLE
static int
_od_mbr_check_service_membership(const uuid_t user, const char *servicename, int *ismember)
{
	xpc_object_t payload, reply;
	int result = EIO;

	if (ismember == NULL || servicename == NULL) return EINVAL;
	
	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (payload == NULL) return EIO;

	MBR_OS_ACTIVITY("Membership API: Validating user is allowed by service");

	xpc_dictionary_set_data(payload, "user_id", user, sizeof(uuid_t));
	xpc_dictionary_set_int64(payload, "user_idtype", ID_TYPE_UUID);
	xpc_dictionary_set_string(payload, "service", servicename);
	
	reply = _od_rpc_call("mbr_check_service_membership", payload, _mbr_xpc_pipe);
	if (reply != NULL) {
		result = (int) xpc_dictionary_get_int64(reply, "error");
		(*ismember) = xpc_dictionary_get_bool(reply, "ismember");
		
		xpc_release(reply);
	} else {
		(*ismember) = 0;
	}
	
	xpc_release(payload);

	return result;
}
#endif

#ifdef DARWIN_DIRECTORY_AVAILABLE
OS_WARN_RESULT
static int
_dd_mbr_check_service_membership(const uuid_t user, const char *servicename, int *ismember)
{
	char groupname[PATH_MAX];

	snprintf(groupname, sizeof(groupname), "com.apple.access_%s", servicename);
	return _dd_mbr_check_membership_ext(ID_TYPE_UUID, user, sizeof(uuid_t), ID_TYPE_GROUPNAME, groupname, 0, ismember);
}
#endif // DARWIN_DIRECTORY_AVAILABLE

LIBINFO_EXPORT
int
mbr_check_service_membership(const uuid_t user, const char *servicename, int *ismember)
{
	int result = EIO;

#ifdef DARWIN_DIRECTORY_AVAILABLE
	if (_darwin_directory_enabled()) {
		result = _dd_mbr_check_service_membership(user, servicename, ismember);
		switch (result) {
			case DD_MBR_SUCCESS:
				return 0;

			case DD_MBR_USER_NOT_FOUND:
				// The user wasn't found in Darwin Directory.  Let OD handle it
				// in case this is a network user.
				break;

			case DD_MBR_GROUP_NOT_FOUND:
				// User was found but the group wasn't.  According to the man
				// page this is an error (which is not the case for regular
				// groups).  Since the user was found in Darwin Directory but
				// the group wasn't return ENOENT.  Local users are not allowed
				// to be in network groups so there's no reason to let OD handle
				// this case.
				return ENOENT;

			case DD_MBR_INVALID_PARAMETER:
			default:
				return EINVAL;
		}
	}
#endif

#ifdef DS_AVAILABLE
	result = _od_mbr_check_service_membership(user, servicename, ismember);
#endif

	return result;
}

#ifdef DS_AVAILABLE
static char *
ConvertBytesToDecimal(char *buffer, unsigned long long value)
{
	char *temp;
	buffer[24] = '\0';
	buffer[23] = '0';

	if (value == 0)
		return &buffer[23];

	temp = &buffer[24];
	while (value != 0)
	{
		temp--;
		*temp = '0' + (value % 10);
		value /= 10;
	}

	return temp;
}
#endif

LIBINFO_EXPORT
int
mbr_sid_to_string(const nt_sid_t *sid, char *string)
{
#ifdef DS_AVAILABLE
	char *current = string;
	long long temp = 0;
	int i;
	char tempBuffer[25];

	if (sid->sid_authcount > NTSID_MAX_AUTHORITIES) return EINVAL;

	for (i = 0; i < 6; i++)
		temp = (temp << 8) | sid->sid_authority[i];

	current[0] = 'S';
	current[1] = '-';
	current += 2;
	strcpy(current, ConvertBytesToDecimal(tempBuffer, sid->sid_kind));
	current = current + strlen(current);
	*current = '-';
	current++;
	strcpy(current, ConvertBytesToDecimal(tempBuffer, temp));

	for(i=0; i < sid->sid_authcount; i++)
	{
		current = current + strlen(current);
		*current = '-';
		current++;
		strcpy(current, ConvertBytesToDecimal(tempBuffer, sid->sid_authorities[i]));
	}

	return 0;
#else
	return EIO;
#endif
}

LIBINFO_EXPORT
int
mbr_string_to_sid(const char *string, nt_sid_t *sid)
{
#ifdef DS_AVAILABLE
	char *current = (char *)string+2;
	int count = 0;
	long long temp;

	if (string == NULL) return EINVAL;

	memset(sid, 0, sizeof(nt_sid_t));
	if (string[0] != 'S' || string[1] != '-') return EINVAL;

	sid->sid_kind = strtol(current, &current, 10);
	if (*current == '\0') return EINVAL;
	current++;
	temp = strtoll(current, &current, 10);

	/* convert to BigEndian before copying */
	temp = OSSwapHostToBigInt64(temp);
	memcpy(sid->sid_authority, ((char*)&temp)+2, 6);
	while (*current != '\0' && count < NTSID_MAX_AUTHORITIES)
	{
		current++;
		errno = 0;
		sid->sid_authorities[count] = (u_int32_t)strtoll(current, &current, 10);
		if ((sid->sid_authorities[count] == 0) && (errno == EINVAL)) {
			return EINVAL;
		}
		count++;
	}

	if (*current != '\0') return EINVAL;

	sid->sid_authcount = count;

	return 0;
#else
	return EIO;
#endif
}

int
mbr_uuid_to_string(const uuid_t uu, char *string)
{
	uuid_unparse_upper(uu, string);
	
	return 0;
}

int
mbr_string_to_uuid(const char *string, uuid_t uu)
{
	return uuid_parse(string, uu);
}

LIBINFO_EXPORT
int 
mbr_set_identifier_ttl(int id_type, const void *identifier, size_t identifier_size, unsigned int seconds)
{
#ifdef DS_AVAILABLE
	xpc_object_t payload, reply;
	int rc = 0;
	
	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (payload == NULL) return ENOMEM;

	MBR_OS_ACTIVITY("Membership API: Change the TTL of a given identifier in SystemCache");

	xpc_dictionary_set_int64(payload, "type", id_type);
	xpc_dictionary_set_data(payload, "identifier", identifier, identifier_size);
	xpc_dictionary_set_int64(payload, "ttl", seconds);
	
	if (rc == 0) {
		reply = _od_rpc_call("mbr_set_identifier_ttl", payload, _mbr_xpc_pipe);
		if (reply != NULL) {
			rc = (int) xpc_dictionary_get_int64(reply, "error");
			xpc_release(reply);
		} else {
			rc = EIO;
		}
	}
	
	xpc_release(payload);
	
	return rc;
#else
	return EIO;
#endif
}
