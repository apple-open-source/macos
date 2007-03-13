/*
 * Copyright (c) 2004-2007 Apple Computer, Inc. All rights reserved.
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

#include "membership.h"
#include "membershipPriv.h"
#include "memberd.h"

#include <sys/errno.h>
#include <servers/bootstrap.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <libkern/OSByteOrder.h>

static mach_port_t GetServerPort()
{
	kern_return_t result;
	static mach_port_t bsPort = 0;
	static mach_port_t fServerPort = 0;

	if (bsPort == 0)
	{
		result = task_get_bootstrap_port(mach_task_self(), &bsPort);
		result = bootstrap_look_up(bsPort, "com.apple.memberd", &fServerPort);
	}

	return fServerPort;
}

int mbr_uid_to_uuid(uid_t id, uuid_t uu)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	int result = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_WANT_UGUID;
	request.el_uid = id;
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_UGUID) != 0)
		memcpy(uu, &request.el_uguid, sizeof(guid_t));
	else
		result = ENOENT;

	return result;
}

int mbr_gid_to_uuid(gid_t id, uuid_t uu)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_WANT_GGUID;
	request.el_gid = id;
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GGUID) != 0)
		memcpy(uu, &request.el_gguid, sizeof(guid_t));
	else
		error = ENOENT;

	return error;
}

int mbr_uuid_to_id(const uuid_t uu, uid_t *id, int *id_type)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_UID | KAUTH_EXTLOOKUP_WANT_GID;
	memcpy(&request.el_uguid, uu, sizeof(guid_t));
	memcpy(&request.el_gguid, uu, sizeof(guid_t));
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

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
		error = ENOENT;
	}

	return error;
}

int mbr_sid_to_uuid(const nt_sid_t *sid, uuid_t uu)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_GSID | KAUTH_EXTLOOKUP_WANT_GGUID;
	memset(&request.el_gsid, 0, sizeof(ntsid_t));
	memcpy(&request.el_gsid, sid, KAUTH_NTSID_SIZE(sid));
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GGUID) != 0)
		memcpy(uu, &request.el_gguid, sizeof(guid_t));
	else
		error = ENOENT;

	return error;
}

int mbr_uuid_to_sid(const uuid_t uu, nt_sid_t *sid)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_GSID;
	memcpy(&request.el_gguid, uu, sizeof(guid_t));
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_GSID) != 0)
		memcpy(sid, &request.el_gsid, sizeof(nt_sid_t));
	else
		error = ENOENT;

	return error;
}

int mbr_check_membership(uuid_t user, uuid_t group, int *ismember)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	memcpy(&request.el_gguid, group, sizeof(guid_t));
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) != 0)
		*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	else
		error = ENOENT;

	return error;
}

int mbr_check_membership_refresh(uuid_t user, uuid_t group, int *ismember)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP | (1 << 15);
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	memcpy(&request.el_gguid, group, sizeof(guid_t));
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) != 0)
		*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	else
		error = ENOENT;

	return error;
}

int mbr_check_membership_by_id(uuid_t user, gid_t group, int *ismember)
{
	struct kauth_identity_extlookup request;
	security_token_t token;
	kern_return_t result;
	int error = 0;

	token.val[0] = -1;
	token.val[1] = -1;

	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	memcpy(&request.el_uguid, user, sizeof(guid_t));
	request.el_gid = group;
	result = _mbr_DoMembershipCall(GetServerPort(), &request, &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) != 0)
		*ismember = ((request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0);
	else
		error = ENOENT;

	return error;
}

int mbr_reset_cache()
{
	security_token_t token;
	kern_return_t result;

	token.val[0] = -1;
	token.val[1] = -1;

	result = _mbr_ClearCache(GetServerPort(), &token);
	if (result != KERN_SUCCESS) return EIO;
	if (token.val[0] != 0) return EAUTH;

	return 0;
}

int mbr_user_name_to_uuid(const char *name, uuid_t uu)
{
	security_token_t token;
	kern_return_t result;

	if (name == NULL) return EINVAL;
	if (strlen(name) > 255) return EINVAL;

	token.val[0] = -1;
	token.val[1] = -1;

	result = _mbr_MapName(GetServerPort(), 1, (char *)name, (guid_t *)uu, &token);
	if (result == KERN_FAILURE) return ENOENT;
	else if (result != KERN_SUCCESS) return EIO;

	if (token.val[0] != 0) return EAUTH;

	return 0;
}

int mbr_group_name_to_uuid(const char *name, uuid_t uu)
{
	security_token_t token;
	kern_return_t result;

	if (name == NULL) return EINVAL;
	if (strlen(name) > 255) return EINVAL;

	token.val[0] = -1;
	token.val[1] = -1;

	result = _mbr_MapName(GetServerPort(), 0, (char *)name, (guid_t *)uu, &token);
	if (result == KERN_FAILURE) return ENOENT;
	else if (result != KERN_SUCCESS) return EIO;

	if (token.val[0] != 0) return EAUTH;

	return 0;
}

int mbr_check_service_membership(const uuid_t user, const char *servicename, int *ismember)
{
	char *prefix = "com.apple.access_";
	char *all_services = "com.apple.access_all_services";
	char groupName[256];
	uuid_t group_uu;
	int result, dummy;

	if (servicename == NULL) return EINVAL;
	if (strlen(servicename) > (255 - strlen(prefix))) return EINVAL;

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
		result = mbr_check_membership_refresh(user, group_uu, ismember);
	}
	else if (result == EAUTH)
	{
		return result;
	}
	else
	{
		/* just force cache update with bogus membership check */
		memset(group_uu, 0, sizeof(group_uu));
		mbr_check_membership_refresh(user, group_uu, &dummy);
	}

	return result;
}

static char *ConvertBytesToDecimal(char *buffer, unsigned long long value)
{
	char *temp;
	buffer[24] = '\0';
	buffer[23] = '0';

	if (value == 0) return &buffer[23];

	temp = &buffer[24];
	while (value != 0)
	{
		temp--;
		*temp = '0' + (value % 10);
		value /= 10;
	}

	return temp;
}

int mbr_sid_to_string(const nt_sid_t *sid, char *string)
{
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

	for (i = 0; i < sid->sid_authcount; i++)
	{
		current = current + strlen(current);
		*current = '-';
		current++;
		strcpy(current, ConvertBytesToDecimal(tempBuffer, sid->sid_authorities[i]));
	}

	return 0;
}

int mbr_string_to_sid(const char *string, nt_sid_t *sid)
{
	char *current = string+2;
	int count = 0;
	long long temp;

	memset(sid, 0, sizeof(nt_sid_t));
	if ((string[0] != 'S') || (string[1] != '-')) return EINVAL;

	sid->sid_kind = strtol(current, &current, 10);
	if (*current == '\0') return EINVAL;
	current++;
	temp = strtoll(current, &current, 10);

	/* convert to BigEndian before copying */
	temp = OSSwapHostToBigInt64(temp);
	memcpy(sid->sid_authority, ((char*)&temp)+2, 6);
	while ((*current != '\0') && (count < NTSID_MAX_AUTHORITIES))
	{
		current++;
		sid->sid_authorities[count] = strtol(current, &current, 10);
		count++;
	}

	if (*current != '\0') return EINVAL;

	sid->sid_authcount = count;

	return 0;
}

static void ConvertBytesToHex(char **string, char **data, int numBytes)
{
	int i;

	for (i = 0; i < numBytes; i++)
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

int mbr_uuid_to_string(const uuid_t uu, char *string)
{
	char *guid = (char *)uu;
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
}

int mbr_string_to_uuid(const char *string, uuid_t uu)
{
	short dataIndex = 0;
	int isFirstNibble = 1;

	if (strlen(string) > MBR_UU_STRING_SIZE)
		return EINVAL;

	while (*string != '\0' && dataIndex < 16)
	{
		char nibble;

		if ((*string >= '0') && (*string <= '9'))
		{
			nibble = *string - '0';
		}
		else if ((*string >= 'A') && (*string <= 'F'))
		{
			nibble = *string - 'A' + 10;
		}
		else if ((*string >= 'a') && (*string <= 'f'))
		{
			nibble = *string - 'a' + 10;
		}
		else
		{
			if (*string != '-') return EINVAL;
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
}
