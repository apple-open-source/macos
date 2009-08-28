/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _MEMBERSHIPPRIV_H_
#define _MEMBERSHIPPRIV_H_

#include <uuid/uuid.h>
#include <ntsid.h>

#define MBR_UU_STRING_SIZE 37
#define MBR_MAX_SID_STRING_SIZE 200

#define SID_TYPE_USER 0
#define SID_TYPE_GROUP 1

__BEGIN_DECLS

int mbr_reset_cache();
int mbr_user_name_to_uuid(const char *name, uuid_t uu);
int mbr_group_name_to_uuid(const char *name, uuid_t uu);
int mbr_check_membership_by_id(uuid_t user, gid_t group, int *ismember);
int mbr_check_membership_refresh(const uuid_t user, uuid_t group, int *ismember);
int mbr_uuid_to_string(const uuid_t uu, char *string);
int mbr_string_to_uuid(const char *string, uuid_t uu);
int mbr_sid_to_string(const nt_sid_t *sid, char *string);
int mbr_string_to_sid(const char *string, nt_sid_t *sid);
int mbr_uuid_to_sid_type(const uuid_t uu, nt_sid_t *sid, int *id_type);

__END_DECLS

#endif /* !_MEMBERSHIPPRIV_H_ */
