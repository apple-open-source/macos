/*
 * opendirectory.c
 *
 * Copyright (C) 2003-2009 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "opendirectory.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static CFDictionaryRef sam_searchattr_first(
				struct opendirectory_session * session,
				const char *	type,
				const char *	attr,
				const char *	value);

static CFDictionaryRef sam_searchname_first(
				struct opendirectory_session * session,
				const char *	type,
				const char *	name);

static void get_sid_for_samrecord(void *mem_ctx,
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *record_sid)
{
	fstring sidstr;
	char * record_name;
	char * record_path;

	record_path = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrMetaNodeLocation);
	record_name = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordName);

	SMB_ASSERT(record_path);
	SMB_ASSERT(record_name);

	DEBUG (5, ("resolving SID for record=%s within %s\n",
		    record_name, record_path));

	/* Local Node - All SIDs are relative to the server SID */
	if (opendirectory_node_path_is_local(session, record_path)) {
		sid_copy(record_sid, get_global_sam_sid());
		DEBUG(4, ("record=%s is local\n", record_name));
		return;
 	}

	if (opendirectory_domain_sid_from_path(mem_ctx, session,
					record_path, record_sid)) {
		DEBUG(4, ("record=%s is relative to domain SID=%s\n",
			    record_name, sid_to_string(sidstr, record_sid)));
	} else {
		/* It's a non-local record, and the node doesn't have
		 * a domain SID, use the well-known synthetic domain SID.
		 */
		string_to_sid(record_sid,
			"S-1-5-21-987654321-987654321-987654321");
		DEBUG(4, ("no domain SID for %s, assuming synthetic SID=%s\n",
			record_path, sid_to_string(sidstr, record_sid)));
	}

}

/* Given a SAM record, use memberd to find the the SID. */
static BOOL
memberd_record_ugid_to_sid(
		void * mem_ctx,
		CFDictionaryRef sam_record,
		const char * attr,
		int id_type,
		DOM_SID * sid)
{
	nt_sid_t ntsid;
	uuid_t uuid;
	id_t ugid;
	int err;
	char * strval;

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, attr);
	if (!strval) {
		return False;
	}

	ugid = (id_t)strtol(strval, NULL, 10 /* base */);

	switch (id_type) {
	case MBR_ID_TYPE_UID:
		err = mbr_uid_to_uuid(ugid, uuid);
		break;
	case MBR_ID_TYPE_GID:
		err = mbr_gid_to_uuid(ugid, uuid);
		break;
	default:
		return False;
	}

	if (err != 0) {
		DEBUG(6, ("failed to map %s %d: %s\n",
			id_type == MBR_ID_TYPE_UID ? "UID" : "GID",
			ugid, strerror(err)));
	}

	err = mbr_uuid_to_sid(uuid, &ntsid);
	if (err != 0) {
		uuid_string_t str;
		uuid_unparse(uuid, str);
		DEBUG(6, ("failed to map %s: %s\n",
			    str, strerror(err)));
		return False;
	}

	convert_ntsid_to_DOMSID(sid, &ntsid);
	return True;
}

/* Given a SAM record, map its UUID (if present) to a SID using memberd. */
static BOOL
memberd_record_uuid_to_sid(
		void * mem_ctx,
		CFDictionaryRef sam_record,
		DOM_SID * sid)
{
	nt_sid_t ntsid;
	uuid_t uuid;
	char * strval;
	int err;

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrGeneratedUID);
	if (!strval) {
		return False;
	}

	if (uuid_parse(strval, uuid) != 0) {
		return False;
	}

	err = mbr_uuid_to_sid(uuid, &ntsid);
	if (err != 0) {
		DEBUG(6, ("failed to map %s: %s\n",
			    strval, strerror(err)));
		return False;
	}

	convert_ntsid_to_DOMSID(sid, &ntsid);
	return True;
}

 BOOL opendirectory_find_usersid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid)
{
	void * mem_ctx = talloc_init("opendirectory_find_usersid_from_record");
	DOM_SID samsid;
	char * strval;
	BOOL ret = False;

	char * record_type;

	/* Make sure we are dealing with a user record. */
	record_type = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordType);
	if (!record_type) {
		goto done;
	}

	DEBUG(6, ("determining user SID for %s record\n", record_type));
	if (strcmp(record_type, kDSStdRecordTypeUsers) != 0 &&
	    strcmp(record_type, kDSStdRecordTypeComputers) != 0) {
		goto done;
	}

	/* Before we start, let's see whether DS has an answer for us. If
	 * there is a UUID on this record, then we ought to be able to map
	 * that. Otherwise, if there is a kDS1AttrUniqueID, we can try to
	 * map that.
	 */

	if (memberd_record_uuid_to_sid(mem_ctx, sam_record, sid)) {
		ret = True;
		goto done;
	}

	if (memberd_record_ugid_to_sid(mem_ctx, sam_record,
			    kDS1AttrUniqueID, MBR_ID_TYPE_UID, sid)) {
		ret = True;
		goto done;
	}

	/* DS didn't find a mapping. Make a best effort ... */

	if (!session->local_path_cache) {
		session->local_path_cache = opendirectory_local_paths(session);
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	/* Since there is no SID attribute available, we will need to
	 * construct something by using an attribute as a RID. We can't do
	 * this unless we have a base SID for this record.
	 */
	get_sid_for_samrecord(mem_ctx, session, sam_record, &samsid);
	sid_copy(sid, &samsid);

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrUniqueID);
	if (strval) {
		int32_t uid = (int32_t)strtol(strval, NULL, 10 /* base */);
		uint32_t rid;

		if (opendirectory_match_record_attribute(sam_record,
				    kDSNAttrRecordName, lp_guestaccount())) {
			rid = DOMAIN_USER_RID_GUEST;
		} else {
			rid = algorithmic_pdb_uid_to_user_rid(uid);
		}

		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	/* Nothing in the directory that we can map this record with. */

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/* Find a primary group SID from a user or computer record.
 *
 * The order we try is
 *	- kDS1AttrSMBPrimaryGroupSID
 *	- kDS1AttrSMBGroupRID
 *	- membership mapping of kDS1AttrPrimaryGroupID
 *	- standard SID of group record from kDS1AttrPrimaryGroupID
 *	- well-known Domain Users and Domain Computers RIDs
 */
static BOOL find_groupsid_from_user(void *mem_ctx,
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *group_sid,
				unsigned fallback_rid)
{
	CFDictionaryRef group_record;
	char * strval;

	DOM_SID samsid;

	/* Both user and group records can have a kDS1AttrSMBPrimaryGroupSID
	 * attribute.
	 */
	strval = opendirectory_get_record_attribute(mem_ctx,
				sam_record, kDS1AttrSMBPrimaryGroupSID);
	if (strval) {
		string_to_sid(group_sid, strval);
		return True;
	}

	/* Since there is no SID attribute available, we will need to
	 * construct something by using an attribute as a RID. We can't do
	 * this unless we have a base SID for this record.
	 */
	get_sid_for_samrecord(mem_ctx, session, sam_record, &samsid);
	sid_copy(group_sid, &samsid);

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record,
					kDS1AttrSMBGroupRID);
	if (strval) {
		uint32_t rid;

		rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(group_sid, rid);
		return True;
	}

	/* This might give the group SID if DS knows how to map the group ID. */
	if (memberd_record_ugid_to_sid(mem_ctx, sam_record,
			kDS1AttrPrimaryGroupID, MBR_ID_TYPE_GID, group_sid)) {
		return True;
	}

	/* The group SID wasn't explicitly overridden, so we have to look up
	 * the group record and work from there.
	 */
	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrPrimaryGroupID);
	if (!strval) {
		return False;
	}

	group_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
			    kDS1AttrPrimaryGroupID, strval);
	if (group_record) {
		BOOL ret;

		ret = opendirectory_find_groupsid_from_record(session,
			    group_record, group_sid);
		CFRelease(group_record);
		return ret;
	}

	/* Nothing in the directory that we can map this record with, but
	 * it's a user record so we can put them in Domain Users.
	 */
	sid_copy(group_sid, get_global_sam_sid());
	sid_append_rid(group_sid, fallback_rid);

	return True;
}

 BOOL opendirectory_find_groupsid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid)
{
	void * mem_ctx = talloc_init("opendirectory_find_groupsid_from_record");
	DOM_SID samsid;
	char * strval;
	BOOL ret = False;

	char * record_type;

	record_type = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordType);
	if (!record_type) {
		goto done;
	}

	DEBUG(6, ("determining group SID for %s record\n", record_type));

	/* If this is a group record, then maybe we can fast-path by just
	 * mapping the UUID to a SID.
	 */
	if (strcmp(record_type, kDSStdRecordTypeGroups) == 0 &&
	    memberd_record_uuid_to_sid(mem_ctx, sam_record, sid)) {
		ret = True;
		goto done;
	}

	if (!session->local_path_cache) {
		session->local_path_cache = opendirectory_local_paths(session);
	}

	/* If we are finding the primary group sid with only a user record,
	 * punt it because we might need to look up the group record.
	 *
	 * We treat computer records the same as user records.
	 */
	if (strcmp(record_type, kDSStdRecordTypeUsers) == 0) {
		ret = find_groupsid_from_user(mem_ctx, session,
					    sam_record, sid,
					    DOMAIN_GROUP_RID_USERS);
		goto done;
	} else if (strcmp(record_type, kDSStdRecordTypeComputers) == 0) {
		ret = find_groupsid_from_user(mem_ctx, session,
						sam_record, sid,
						DOMAIN_GROUP_RID_COMPUTERS);
		goto done;
	}

	/* From now on, we only deal with groups ... */
	if (strcmp(record_type, kDSStdRecordTypeGroups) != 0) {
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
				sam_record, kDS1AttrSMBPrimaryGroupSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	/* Since there is no SID attribute available, we will need to
	 * construct something by using an attribute as a RID. We can't do
	 * this unless we have a base SID for this record.
	 */
	get_sid_for_samrecord(mem_ctx, session, sam_record, &samsid);
	sid_copy(sid, &samsid);

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBGroupRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	/* For all types of groups, this might give the group SID
	 * if DS knows how to map the group ID.
	 */
	if (memberd_record_ugid_to_sid(mem_ctx, sam_record,
			    kDS1AttrPrimaryGroupID, MBR_ID_TYPE_GID, sid)) {
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrPrimaryGroupID);
	if (strval) {
		int32_t gid = (int32_t)strtol(strval, NULL, 10 /* base */);
		uint32_t rid;

		/* Note that this implies that we have matching user and group
		 * names for lp_guestaccount(). This is true for common choices
		 * like "unknown" and "nobody".
		 */
		if (opendirectory_match_record_attribute(sam_record,
				    kDSNAttrRecordName, lp_guestaccount())) {
			rid = DOMAIN_GROUP_RID_GUESTS;
		} else {
			rid = algorithmic_pdb_gid_to_group_rid(gid);
		}

		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	if (strcmp(record_type, kDSStdRecordTypeComputers) == 0) {
		sid_append_rid(sid, DOMAIN_GROUP_RID_COMPUTERS);
		ret = True;
	}

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/* Search for a user or group record when we have the user of group ID. We do
 * the search twice, once for unsigned formatting, once for signed.
 */
 CFDictionaryRef opendirectory_sam_searchugid_first(
				struct opendirectory_session *session,
				const char *type,
				const char *attr,
				const id_t ugid)
{
	char buf[64];
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	snprintf(buf, sizeof(buf), "%u", ugid);

	status = opendirectory_sam_searchattr(session,
			    &records, type, attr, buf);

	if (status == eDSNoErr && records == NULL) {
		/* Hmmm ... no records? Let's try a signed search .. */

		snprintf(buf, sizeof(buf), "%d", ugid);

		status = opendirectory_sam_searchattr(session,
				    &records, type, attr, buf);
	}

	if (status == eDSNoErr && records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef sam_searchuuid_first(
				struct opendirectory_session *session,
				const char *type,
				const uuid_t uuid)
{
	uuid_string_t str;
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	uuid_unparse_upper(uuid, str);
	status = opendirectory_sam_searchattr(session,
			    &records, type, kDS1AttrGeneratedUID, str);

	if (status == eDSNoErr && records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef sam_searchname_first(
				struct opendirectory_session *session,
				const char *type,
				const char *name)
{
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	status = opendirectory_sam_searchname(session,
			    &records, type, name);

	if (status == eDSNoErr && records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef sam_searchattr_first(
				struct opendirectory_session *session,
				const char *type,
				const char *attr,
				const char *value)
{
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	status = opendirectory_sam_searchattr(session,
			    &records, type, attr, value);

	if (status == eDSNoErr && records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef find_record_from_usersid_and_domsid(
				struct opendirectory_session *session,
				const char *domain_name __unused,
				const DOM_SID *domain_sid,
				const void *data)
{
	CFDictionaryRef sam_record;
	fstring rid_string;
	fstring uid_string;
	uint32_t rid;

	const DOM_SID * sid = (const DOM_SID *)data;

	if (!sid_peek_check_rid(domain_sid, sid, &rid)) {
		return NULL;
	}

	if (rid == DOMAIN_USER_RID_GUEST) {
		return sam_searchname_first(session, kDSStdRecordTypeUsers,
					 lp_guestaccount());
	}

	snprintf(rid_string, sizeof(rid_string) - 1, "%u", rid);
	snprintf(uid_string, sizeof(uid_string) - 1, "%u",
			    algorithmic_pdb_user_rid_to_uid(rid));

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	/* If it's not a generated user RID, searching won't help. */
	if (!algorithmic_pdb_rid_is_user(rid)) {
		return NULL;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrUniqueID, uid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrUniqueID, uid_string);
	if (sam_record) {
		return sam_record;
	}

	return NULL;
}

/* Map SIDS that are relative to the SAM SID to the "well-known" Apple builtin
 * SID of the form S-1-5-21-RID. Arguably we should also do this for the domain
 * SID if we are a domain controller.
 */
static BOOL apple_wellknown_sid(const DOM_SID *sid, DOM_SID *apple_sid)
{
	uint32_t rid;
	DOM_SID apple_wellknown =
	    { 1, 1, {0,0,0,0,0,5}, {21,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

	if (!sid_peek_check_rid(get_global_sam_sid(), sid, &rid)) {
		return False;
	}

	return sid_compose(apple_sid, &apple_wellknown, rid);
}

/* Find a user or group record from a SID using membership routines. */
static CFDictionaryRef memberd_find_record_from_sid(
				struct opendirectory_session *session,
				const int id_type,
				const DOM_SID *sid)
{
	id_t ugid;
	int ugid_type;
	uuid_t uuid;
	int err;

	const char * type =
	    (id_type == MBR_ID_TYPE_UID ? kDSStdRecordTypeUsers
					: kDSStdRecordTypeGroups);

	const char * ugid_attr =
	    (id_type == MBR_ID_TYPE_UID ? kDS1AttrUniqueID
					: kDS1AttrPrimaryGroupID);

	CFDictionaryRef sam_record = NULL;

	SMB_ASSERT(id_type == MBR_ID_TYPE_UID || id_type == MBR_ID_TYPE_GID);

	/* First, convert the SID to a UUID. */
	if (!memberd_sid_to_uuid(sid, uuid)) {
		return NULL;
	}

	/* If it's a compatibility UUID, then try to search for a
	 * user or group with that ID.
	 */
	if (is_compatibility_guid(uuid, &ugid_type, &ugid) &&
	    ugid_type == id_type) {
		DEBUG(6, ("searching compatibility ugid type=%d, ugid=%u\n",
			id_type, ugid));
		sam_record = opendirectory_sam_searchugid_first(session,
					type, ugid_attr, ugid);
	} else {
		/* Otherwise, we ought to be able to just search by the UUID. */
		sam_record = sam_searchuuid_first(session, type, uuid);
	}

	if (sam_record) {
		return sam_record;
	}

	/* If we didn't find it, map the group UUID to a gid_t or uid_t
	 * and search for the record with that ID.
	 */
	err = mbr_uuid_to_id(uuid, &ugid, &ugid_type);
	if (err) {
		uuid_string_t str;
		uuid_unparse_upper(uuid, str);
		DEBUG(6, ("unable to map %s to an ID: %s\n",
			str, strerror(err)));
		return NULL;
	}

	if (ugid_type == id_type) {
		sam_record = opendirectory_sam_searchugid_first(session,
					type, ugid_attr, ugid);
	}

	return sam_record;
}

 CFDictionaryRef opendirectory_find_record_from_usersid(
				struct opendirectory_session *session,
				const DOM_SID *sid)
{
	CFDictionaryRef sam_record;
	fstring sid_string;

	DOM_SID domain_sid = {0};
	DOM_SID apple_user_sid = {0};
	DOM_SID sam_sid = {0};

	sam_record = memberd_find_record_from_sid(session,
			MBR_ID_TYPE_UID, sid);
	if (sam_record) {
		return sam_record;
	}

	secrets_fetch_domain_sid(lp_workgroup(), &domain_sid);
	sam_sid = *get_global_sam_sid();

	sid_to_string(sid_string, sid);

	DEBUG(6, ("searching for user SID %s the hard way\n",
		    sid_string));

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	/* Check whether this SID is an Apple "well-known" SID. Since there are
	 * no well-known Computers, we don't need to repeat this check.
	 */
	if (apple_wellknown_sid(sid, &apple_user_sid)) {
		sid_to_string(sid_string, &apple_user_sid);
		sam_record = sam_searchattr_first(session,
					kDSStdRecordTypeUsers,
					kDS1AttrSMBSID, sid_string);
		if (sam_record) {
			return sam_record;
		}
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	/* The SID might be in a domain we know about, so we can try poking
	 * around for something to match the RID.
	 */

	sam_record = find_record_from_usersid_and_domsid(session,
						    NULL, &sam_sid, sid);
	if (sam_record) {
		return sam_record;
	}

	/* Also try the domain SID if it is not the same as our SAM SID. */
	if (sid_compare_domain(&domain_sid, &sam_sid) != 0) {
		sam_record = find_record_from_usersid_and_domsid(session,
						    NULL, &domain_sid, sid);
		if (sam_record) {
			return sam_record;
		}
	}

	/* As a last resort, iterate over all the domains that are available in
	 * the directory and see whether we can match the SID in any of those.
	 */
	if (session->domain_sid_cache == NULL) {
		void * mem_ctx = talloc_init(__FUNCTION__);
		opendirectory_fill_domain_sid_cache(mem_ctx, session);
		TALLOC_FREE(mem_ctx);
	}

	return opendirectory_search_domain_sid_cache(session, sid,
		    find_record_from_usersid_and_domsid);
}

static CFDictionaryRef find_record_from_groupsid_and_domsid(
				struct opendirectory_session *session,
				const char *domain_name __unused,
				const DOM_SID *domain_sid,
				const void *match_data)
{
	CFDictionaryRef sam_record;
	fstring rid_string;
	fstring gid_string;
	uint32_t rid;

	const DOM_SID *group_sid = (const DOM_SID *)match_data;

	if (!sid_peek_check_rid(domain_sid, group_sid, &rid)) {
		return NULL;
	}

	DEBUG(8, ("searching domain %s for group record with SID %s\n",
		sid_to_string(gid_string, domain_sid),
		sid_to_string(rid_string, group_sid)));

	snprintf(rid_string, sizeof(rid_string) - 1, "%u", rid);

	/* First, search for a record with a matching group RID. If that fails,
	 * we can check to see whether it's a well-known RID for which we have
	 * a builtin default.
	 */
	sam_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBGroupRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	switch (rid) {
	case DOMAIN_GROUP_RID_USERS:
	case BUILTIN_ALIAS_RID_USERS:
		/* The local group "staff" has a SID of S-1-5-32-545, which is
		 * the well-known SID for "Users". Additionally, the "staff"
		 * group has a RealName attribute of "Users", so this seems
		 * like a good match.
		 */
		return sam_searchname_first(session, kDSStdRecordTypeGroups,
					 "staff");
	case DOMAIN_GROUP_RID_GUESTS:
	case BUILTIN_ALIAS_RID_GUESTS:
	case DOMAIN_GROUP_RID_COMPUTERS:
		/* The default config has "guest account = nobody", so we can
		 * use this for both the guest user and the guest group. We
		 * can't guarantee that the guest account is also a group
		 * however.
		 *
		 * Computer accounts get mapped to nobody as well because they
		 * are untrusted as far as filesystem access goes.
		 */
		sam_record = sam_searchname_first(session,
			    kDSStdRecordTypeGroups, lp_guestaccount());

		if (!sam_record) {
			sam_record = sam_searchname_first(session,
					kDSStdRecordTypeGroups, "nobody");
		}

		return sam_record;

	case DOMAIN_GROUP_RID_ADMINS:
	case DOMAIN_GROUP_RID_CONTROLLERS:
	case DOMAIN_GROUP_RID_CERT_ADMINS:
	case DOMAIN_GROUP_RID_SCHEMA_ADMINS:
	case DOMAIN_GROUP_RID_ENTERPRISE_ADMINS:
	case BUILTIN_ALIAS_RID_ADMINS:
	case BUILTIN_ALIAS_RID_POWER_USERS:
		return sam_searchname_first(session, kDSStdRecordTypeGroups,
					 "admin");
	}

	if (rid >= BASE_RID) {
		id_t ugid = pdb_group_rid_to_gid(rid);

		/* If it a generated user RID, searching won't help because
		 * we are looking for a group.
		 */
		if (algorithmic_pdb_rid_is_user(rid)) {
			return NULL;
		}

		sam_record = opendirectory_sam_searchugid_first(session,
					kDSStdRecordTypeGroups,
					kDS1AttrPrimaryGroupID,
					ugid);
		if (sam_record) {
			return sam_record;
		}
	}

	return NULL;
}

 CFDictionaryRef opendirectory_find_record_from_groupsid(
				struct opendirectory_session *session,
				const DOM_SID *group_sid)
{
	CFDictionaryRef sam_record;
	fstring sid_string;

	DOM_SID domain_sid = {0};
	DOM_SID sam_sid = {0};
	DOM_SID apple_group_sid = {0};

	sam_record = memberd_find_record_from_sid(session,
			MBR_ID_TYPE_GID, group_sid);
	if (sam_record) {
		return sam_record;
	}

	sid_to_string(sid_string, group_sid);
	secrets_fetch_domain_sid(lp_workgroup(), &domain_sid);
	sam_sid = *get_global_sam_sid();

	DEBUG(6, ("searching for group SID %s the hard way\n",
		    sid_string));

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	if (apple_wellknown_sid(group_sid, &apple_group_sid)) {
		sid_to_string(sid_string, &apple_group_sid);
		sam_record = sam_searchattr_first(session,
					kDSStdRecordTypeGroups,
					kDS1AttrSMBSID, sid_string);
		if (sam_record) {
			return sam_record;
		}
	}

	sam_record = find_record_from_groupsid_and_domsid(session,
				NULL, &sam_sid, group_sid);
	if (sam_record) {
		return sam_record;
	}

	if (sid_compare(&domain_sid, &sam_sid) != 0) {
		sam_record = find_record_from_groupsid_and_domsid(session,
					    NULL, &domain_sid, group_sid);
		if (sam_record) {
			return sam_record;
		}
	}

	if (session->domain_sid_cache == NULL) {
		void * mem_ctx = talloc_init(__FUNCTION__);
		opendirectory_fill_domain_sid_cache(mem_ctx, session);
		TALLOC_FREE(mem_ctx);
	}

	return opendirectory_search_domain_sid_cache(session, group_sid,
		    find_record_from_groupsid_and_domsid);
}

