/*
 * Copyright (c) 2004-2010 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <sys/errno.h>
#include <mach/mach.h>
#include "membership.h"
#include "membershipPriv.h"
#include <servers/bootstrap.h>
#include <libkern/OSByteOrder.h>
#ifdef DS_AVAILABLE
#include "DSmemberdMIG.h"
#endif

#ifdef DS_AVAILABLE
extern mach_port_t _mbr_port;
extern int _ds_running(void);

static const uuid_t _user_compat_prefix = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};
static const uuid_t _group_compat_prefix = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0x00, 0x00, 0x00, 0x00};

#define COMPAT_PREFIX_LEN	(sizeof(uuid_t) - sizeof(id_t))

#define MAX_LOOKUP_ATTEMPTS 10
#endif

uid_t
audit_token_uid(audit_token_t a)
{
	/*
	 * This should really call audit_token_to_au32,
	 * but that's in libbsm, not in a Libsystem library.
	 */
	return (uid_t)a.val[1];
}

#ifdef DS_AVAILABLE
static int
_mbr_MembershipCall(struct kauth_identity_extlookup *req)
{
	audit_token_t token;
	kern_return_t status;
	uint32_t i;

	/* call _ds_running() to look up _mbr_port */
	_ds_running();
	if (_mbr_port == MACH_PORT_NULL) return EIO;

	memset(&token, 0, sizeof(audit_token_t));

	status = MIG_SERVER_DIED;
	for (i = 0; (_mbr_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (i < MAX_LOOKUP_ATTEMPTS); i++)
	{
		status = memberdDSmig_MembershipCall(_mbr_port, req, &token);
		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);
			_mbr_port = MACH_PORT_NULL;
			_ds_running();
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS) return EIO;
	if (audit_token_uid(token) != 0) return EAUTH;

	return 0;
}
#endif

#ifdef DS_AVAILABLE
static int
_mbr_MapName(char *name, int type, guid_t *uu)
{
	kern_return_t status;
	audit_token_t token;
	uint32_t i;

	if (name == NULL) return EINVAL;
	if (strlen(name) > 255) return EINVAL;

	/* call _ds_running() to look up _mbr_port */
	_ds_running();
	if (_mbr_port == MACH_PORT_NULL) return EIO;

	memset(&token, 0, sizeof(audit_token_t));

	status = MIG_SERVER_DIED;
	for (i = 0; (_mbr_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (i < MAX_LOOKUP_ATTEMPTS); i++)
	{
		status = memberdDSmig_MapName(_mbr_port, type, name, uu, &token);
		if (status == KERN_FAILURE) return ENOENT;

		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);
			_mbr_port = MACH_PORT_NULL;
			_ds_running();
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS) return EIO;
	if (audit_token_uid(token) != 0) return EAUTH;

	return 0;
}
#endif

#ifdef DS_AVAILABLE
static int
_mbr_ClearCache()
{
	kern_return_t status;
	uint32_t i;

	/* call _ds_running() to look up _mbr_port */
	_ds_running();
	if (_mbr_port == MACH_PORT_NULL) return EIO;

	status = MIG_SERVER_DIED;
	for (i = 0; (_mbr_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (i < MAX_LOOKUP_ATTEMPTS); i++)
	{
		status = memberdDSmig_ClearCache(_mbr_port);
		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);
			_mbr_port = MACH_PORT_NULL;
			_ds_running();
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS) return EIO;

	return 0;
}
#endif

#ifdef DS_AVAILABLE
static int
_mbr_SetIdentifierTTL(int idType, const void *identifier, size_t identifier_size, unsigned int seconds)
{
	kern_return_t status;
	uint32_t i;
	
	/* call _ds_running() to look up _mbr_port */
	_ds_running();
	if (_mbr_port == MACH_PORT_NULL) return EIO;
	
	status = MIG_SERVER_DIED;
	for (i = 0; (_mbr_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (i < MAX_LOOKUP_ATTEMPTS); i++)
	{
		status = memberdDSmig_SetIdentifierTTL(_mbr_port, idType, (identifier_data_t)identifier, identifier_size, seconds);
		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);
			_mbr_port = MACH_PORT_NULL;
			_ds_running();
			status = MIG_SERVER_DIED;
		}
	}
	
	if (status != KERN_SUCCESS) return EIO;
	
	return 0;
}
#endif

int
mbr_uid_to_uuid(uid_t id, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_UID, &id, sizeof(id), uu);
}

int
mbr_gid_to_uuid(gid_t id, uuid_t uu)
{
	return mbr_identifier_to_uuid(ID_TYPE_GID, &id, sizeof(id), uu);
}

int
mbr_uuid_to_id(const uuid_t uu, uid_t *id, int *id_type)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;
	id_t tempID;

	if (id == NULL) return EIO;
	if (id_type == NULL) return EIO;

	if (!memcmp(uu, _user_compat_prefix, COMPAT_PREFIX_LEN))
	{
		memcpy(&tempID, &uu[COMPAT_PREFIX_LEN], sizeof(tempID));
		*id = ntohl(tempID);
		*id_type = ID_TYPE_UID;
		return 0;
	}
	else if (!memcmp(uu, _group_compat_prefix, COMPAT_PREFIX_LEN))
	{
		memcpy(&tempID, &uu[COMPAT_PREFIX_LEN], sizeof(tempID));
		*id = ntohl(tempID);
		*id_type = ID_TYPE_GID;
		return 0;
	}

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_UID | KAUTH_EXTLOOKUP_WANT_GID;
	memcpy(&request.el_uguid, uu, sizeof(guid_t));
	memcpy(&request.el_gguid, uu, sizeof(guid_t));

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_UID) != 0)
	{
		*id = request.el_uid;
		*id_type = ID_TYPE_UID;
	}
	else if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GID) != 0)
	{
		*id = request.el_gid;
		*id_type = ID_TYPE_GID;
	}
	else
	{
		return ENOENT;
	}

	return 0;
#else
	return EIO;
#endif
}

int
mbr_sid_to_uuid(const nt_sid_t *sid, uuid_t uu)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_GSID | KAUTH_EXTLOOKUP_WANT_GGUID | KAUTH_EXTLOOKUP_VALID_USID | KAUTH_EXTLOOKUP_WANT_UGUID;
	memset(&request.el_gsid, 0, sizeof(ntsid_t));
	memcpy(&request.el_gsid, sid, KAUTH_NTSID_SIZE(sid));
	memset(&request.el_usid, 0, sizeof(ntsid_t));
	memcpy(&request.el_usid, sid, KAUTH_NTSID_SIZE(sid));

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GGUID) != 0) memcpy(uu, &request.el_gguid, sizeof(guid_t));
	else if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_UGUID) != 0) memcpy(uu, &request.el_uguid, sizeof(guid_t));
	else return ENOENT;

	return 0;
#else
	return EIO;
#endif
}

int
mbr_identifier_to_uuid(int id_type, const void *identifier, size_t identifier_size, uuid_t uu)
{
#ifdef DS_AVAILABLE
	kern_return_t status;
	audit_token_t token;
	vm_offset_t ool = 0;
	mach_msg_type_number_t oolCnt = 0;
	uint32_t i;
	id_t tempID;
#if __BIG_ENDIAN__
	id_t newID;
#endif

	if (identifier == NULL) return EINVAL;
	if (identifier_size == 0) return EINVAL;
	else if (identifier_size == -1) identifier_size = strlen((char*) identifier) + 1;

	/* call _ds_running() to look up _mbr_port */
	_ds_running();

	/* if this is a UID or GID translation, we shortcut UID/GID 0 */
	/* if no DS, we return compatibility UUIDs */
	switch (id_type)
	{
		case ID_TYPE_UID:
		{
			if (identifier_size != sizeof(tempID)) return EINVAL;

			tempID = *((id_t *) identifier);
			if ((tempID == 0) || (_mbr_port == MACH_PORT_NULL))
			{
				uuid_copy(uu, _user_compat_prefix);
				*((id_t *) &uu[COMPAT_PREFIX_LEN]) = htonl(tempID);
				return 0;
			}
			break;
		}
		case ID_TYPE_GID:
		{
			if (identifier_size != sizeof(tempID)) return EINVAL;

			tempID = *((id_t *) identifier);
			if ((tempID == 0) || (_mbr_port == MACH_PORT_NULL))
			{
				uuid_copy(uu, _group_compat_prefix);
				*((id_t *) &uu[COMPAT_PREFIX_LEN]) = htonl(tempID);
				return 0;
			}
			break;
		}
	}

	if (_mbr_port == MACH_PORT_NULL) return EIO;

	memset(&token, 0, sizeof(audit_token_t));

#if __BIG_ENDIAN__
	switch (id_type)
	{
		case ID_TYPE_UID:
		case ID_TYPE_GID:
			if (identifier_size < sizeof(id_t)) return EINVAL;
			newID = OSSwapInt32(*((id_t *) identifier));
			identifier = &newID;
			break;
	}
#endif

	if (identifier_size > MAX_MIG_INLINE_DATA)
	{
		if (vm_read(mach_task_self(), (vm_offset_t) identifier, identifier_size, &ool, &oolCnt) != 0) return ENOMEM;
		identifier = NULL;
		identifier_size = 0;
	}

	status = MIG_SERVER_DIED;
	for (i = 0; (_mbr_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (i < MAX_LOOKUP_ATTEMPTS); i++)
	{
		status = memberdDSmig_MapIdentifier(_mbr_port, id_type, (identifier_data_t) identifier, identifier_size, ool, oolCnt, (guid_t *)uu, &token);
		if (status == KERN_FAILURE) return ENOENT;

		if (status == MACH_SEND_INVALID_DEST)
		{
			if (ool != 0) vm_deallocate(mach_task_self(), ool, oolCnt);

			mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);
			_mbr_port = MACH_PORT_NULL;
			_ds_running();
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS) return EIO;
	if (audit_token_uid(token) != 0) return EAUTH;

	return 0;
#else
	return EIO;
#endif
}

int
mbr_uuid_to_sid_type(const uuid_t uu, nt_sid_t *sid, int *id_type)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_USID | KAUTH_EXTLOOKUP_WANT_GSID;
	memcpy(&request.el_uguid, uu, sizeof(guid_t));
	memcpy(&request.el_gguid, uu, sizeof(guid_t));

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_USID) != 0)
	{
		*id_type = SID_TYPE_USER;
		memcpy(sid, &request.el_usid, sizeof(nt_sid_t));
	}
	else if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GSID) != 0)
	{
		*id_type = SID_TYPE_GROUP;
		memcpy(sid, &request.el_gsid, sizeof(nt_sid_t));
	}
	else
	{
		return ENOENT;
	}

	return 0;
#else
	return EIO;
#endif
}

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

int
mbr_check_membership(const uuid_t user, const uuid_t group, int *ismember)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	memcpy(&request.el_gguid, group, sizeof(guid_t));

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;
	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) == 0) return ENOENT;

	*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	return 0;
#else
	return EIO;
#endif
}

int
mbr_check_membership_refresh(const uuid_t user, uuid_t group, int *ismember)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP | (1<<15);
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	memcpy(&request.el_gguid, group, sizeof(guid_t));

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;
	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) == 0) return ENOENT;

	*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	return 0;
#else
	return EIO;
#endif
}

int
mbr_check_membership_by_id(uuid_t user, gid_t group, int *ismember)
{
#ifdef DS_AVAILABLE
	struct kauth_identity_extlookup request;
	int status;

	request.el_seqno = 1;
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	request.el_gid = group;

	status = _mbr_MembershipCall(&request);
	if (status != 0) return status;
	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) == 0) return ENOENT;

	*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	return 0;
#else
	return EIO;
#endif
}

int
mbr_reset_cache()
{
#ifdef DS_AVAILABLE
	return _mbr_ClearCache();
#else
	return EIO;
#endif
}

int
mbr_user_name_to_uuid(const char *name, uuid_t uu)
{
#ifdef DS_AVAILABLE
	return _mbr_MapName((char *)name, 1, (guid_t *)uu);
#else
	return EIO;
#endif
}

int
mbr_group_name_to_uuid(const char *name, uuid_t uu)
{
#ifdef DS_AVAILABLE
	return _mbr_MapName((char *)name, 0, (guid_t *)uu);
#else
	return EIO;
#endif
}

int
mbr_check_service_membership(const uuid_t user, const char *servicename, int *ismember)
{
#ifdef DS_AVAILABLE
	char *prefix = "com.apple.access_";
	char *all_services = "com.apple.access_all_services";
	char groupName[256];
	uuid_t group_uu;
	int result;

	if (servicename == NULL) return EINVAL;
	if (strlen(servicename) > 255 - strlen(prefix)) return EINVAL;

	/* start by checking "all services" */
	result = mbr_group_name_to_uuid(all_services, group_uu);

	if (result == EAUTH) return result;

	if (result == ENOENT)
	{
		/* all_services group didn't exist, check individual group */
		memcpy(groupName, prefix, strlen(prefix));
		strcpy(groupName + strlen(prefix), servicename);
		result = mbr_group_name_to_uuid(groupName, group_uu);
	}

	if (result == 0)
	{
		/* refreshes are driven at a higher level, just check membership */
		result = mbr_check_membership(user, group_uu, ismember);
	}

	return result;
#else
	return EIO;
#endif
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

#ifdef DS_AVAILABLE
static void
ConvertBytesToHex(char **string, char **data, int numBytes)
{
	int i;

	for (i=0; i < numBytes; i++)
	{
		unsigned char hi = ((**data) >> 4) & 0xf;
		unsigned char low = (**data) & 0xf;
		if (hi < 10)
			**string = '0' + hi;
		else
			**string = 'A' + hi - 10;

		(*string)++;

		if (low < 10)
			**string = '0' + low;
		else
			**string = 'A' + low - 10;

		(*string)++;
		(*data)++;
	}
}
#endif

int
mbr_uuid_to_string(const uuid_t uu, char *string)
{
#ifdef DS_AVAILABLE
	char *guid = (char*)uu;
	char *strPtr = string;
	ConvertBytesToHex(&strPtr, &guid, 4);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 6);
	*strPtr = '\0';

	return 0;
#else
	return EIO;
#endif
}

int
mbr_string_to_uuid(const char *string, uuid_t uu)
{
#ifdef DS_AVAILABLE
	short dataIndex = 0;
	int isFirstNibble = 1;

	if (string == NULL) return EINVAL;
	if (strlen(string) > MBR_UU_STRING_SIZE) return EINVAL;

	while (*string != '\0' && dataIndex < 16)
	{
		char nibble;

		if (*string >= '0' && *string <= '9')
			nibble = *string - '0';
		else if (*string >= 'A' && *string <= 'F')
			nibble = *string - 'A' + 10;
		else if (*string >= 'a' && *string <= 'f')
			nibble = *string - 'a' + 10;
		else
		{
			if (*string != '-')
				return EINVAL;
			string++;
			continue;
		}

		if (isFirstNibble)
		{
			uu[dataIndex] = nibble << 4;
			isFirstNibble = 0;
		}
		else
		{
			uu[dataIndex] |= nibble;
			dataIndex++;
			isFirstNibble = 1;
		}

		string++;
	}

	if (dataIndex != 16) return EINVAL;

	return 0;
#else
	return EIO;
#endif
}

int 
mbr_set_identifier_ttl(int id_type, const void *identifier, size_t identifier_size, unsigned int seconds)
{
#ifdef DS_AVAILABLE
	_mbr_SetIdentifierTTL(id_type, identifier, identifier_size, seconds);
	return 0;
#else
	return EIO;
#endif
}
