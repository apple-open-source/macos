/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>

#include "macros.hpp"
#include "lib/common.hpp"
#include "lib/SmbConfig.hpp"

#include <sys/stat.h>
#include <sysexits.h>
#include <iostream>

static struct {
    CFStringRef dsname;
    CFTypeID cftype;
    const char * smbname;
} share_params[] = 
{
    { CFSTR("dsAttrTypeNative:name"), CFStringGetTypeID(),
		    "comment" },
    { CFSTR("dsAttrTypeNative:directory_path"), CFStringGetTypeID(),
		    "path" },
    { CFSTR("dsAttrTypeNative:smb_shared"), CFBooleanGetTypeID(),
		    "available" },
    { CFSTR("dsAttrTypeNative:smb_guestaccess"), CFBooleanGetTypeID(),
		    "guest ok" },
    { CFSTR("dsAttrTypeNative:smb_inherit_permissions"), CFBooleanGetTypeID(),
		    "inherit permissions" },
    { CFSTR("dsAttrTypeNative:smb_createmask"), CFStringGetTypeID(),
		    "create mask" },
    { CFSTR("dsAttrTypeNative:smb_directorymask"), CFStringGetTypeID(),
		    "directory mask" },
    { CFSTR("dsAttrTypeNative:smb_oplocks"), CFBooleanGetTypeID(),
		    "oplocks" },
    { CFSTR("dsAttrTypeNative:smb_strictlocking"), CFStringGetTypeID(),
		    "strict locking" },
};

bool
CopyFirstAttrFromRecord(CFStringRef * val,
			ODRecordRef record,
			CFStringRef attribute)
{
    cf_typeref<CFArrayRef>
	values(ODRecordCopyValues(record, attribute, NULL /* &error */));

    if (!values || CFArrayGetCount(values) == 0) {
	return false;
    }

    assert(CFArrayGetCount(values) >= 1);
    *val = (CFStringRef)CFArrayGetValueAtIndex(values, 0);
    CFRetain(*val);

    return true;
}

bool GetStringAttrFromRecord(std::string& str,
	ODRecordRef record, CFStringRef attribute)
{
    bool status = true;
    CFStringRef attrval = NULL;

    if (!CopyFirstAttrFromRecord(&attrval, record, attribute)) {
	return false;
    }

    str = cfstring_convert(attrval);
    if (str.size() == 0){
	status = false;
    }

    safe_release(attrval);
    return status;
}

static bool convert_to_bool(const std::string& strval)
{
    if (strval == "1" || strval == "yes" || strval == "true") {
	return true;
    }

    return false;
}

static void insert_share_record(SmbShares& shares, ODRecordRef r)
{
    bool default_guest = true;

    std::string name;
    std::string path;

    if (!GetStringAttrFromRecord(name, r, CFSTR("dsAttrTypeNative:smb_name"))) {
	GetStringAttrFromRecord(name, r, CFSTR("dsAttrTypeNative:name"));
    }

    GetStringAttrFromRecord(path, r, CFSTR("dsAttrTypeNative:directory_path"));

    // A valid share must have a name and a path ....
    if (name.size() == 0 || path.size() == 0) {
	return;
    }

    for (unsigned i = 0;
	    i < (sizeof(share_params) / sizeof(share_params[0]));
	    ++i) {
	std::string val;

	if (!GetStringAttrFromRecord(val, r, share_params[i].dsname)) {
	    continue;
	}

	if (share_params[i].cftype == CFBooleanGetTypeID()) {
	    // Special case boolean options to ensure consistent formatting.
	    shares.set_param(name,
		    make_smb_param(share_params[i].smbname,
			convert_to_bool(val)));
	} else {
	    shares.set_param(name,
		    make_smb_param(share_params[i].smbname, val));
	}

	if (strcmp(share_params[i].smbname, "guest ok") == 0) {
	    default_guest = false;
	}
    }

    // Adding options multiple times results in duplicates in the config file.
    // This is OK but ugly, so we only default "guest ok" if it wasn't already
    // set.
    if (default_guest) {
	shares.set_param(name, make_smb_param("guest ok", Options::DefaultGuest));
    }

    shares.set_param(name, make_smb_param("read only", false));
}

static CFArrayRef query_shares(void)
{
    CFArrayRef result;

    cf_typeref<ODQueryRef> q(ODQueryCreateWithNodeType(
		kCFAllocatorDefault,
		kODNodeTypeLocalNodes,
		kODRecordTypeSharePoints,
		NULL /* attribute name to query */,
		kODMatchEqualTo /* WTF is this when we want all records? */,
		NULL /* attribute value to match */,
		NULL /* attribute names to return */,
		(CFIndex)-1 /* results count */,
		NULL /* &err */));

    if (!q) {
	return NULL;
    }

    result = ODQueryCopyResults(q, false /* no partial */, NULL /* &err */);
    if (!result) {
	return NULL;
    }

    if (CFArrayGetCount(result) == 0) {
	safe_release(result);
	return NULL;
    }

    return result;
}

/* Print the current settings in smb.conf format. */
static int cmd_list_pending(SmbShares& shares)
{
    shares.format(std::cout);
    return EX_OK;
}

/* Synchronise the SMB smb shares with DS local. */
static int cmd_sync_shares(SmbShares& shares)
{
    SyncMutex mutex("/var/samba/shares.mutex");

    if (!mutex || getuid() != 0) {
	VERBOSE("only root can synchronize SMB shares\n");
	return EX_NOPERM;
    }

    VERBOSE("rewriting SMB shares\n");

    for (int tries = 5; tries; --tries) {
	if (shares.writeback()) {
	    post_service_notification("sharepoints", NULL);
	    return EX_OK;
	}
    }

    VERBOSE("failed to write new SMB configuration\n");
    return EX_CANTCREAT;
}

int main(int argc, char * const * argv)
{
    CFArrayRef result;
    SmbShares shares;

    setprogname("smb-sync-shares");
    umask(0);

    Options::parse_shares(argc, argv);

    result = query_shares();
    if (result) {
	VERBOSE("synchronizing %u results\n",
		(unsigned)CFArrayGetCount(result));
    } else {
	return EX_OSERR;
    }

    for (unsigned i = 0; i < (unsigned)CFArrayGetCount(result); ++i) {
	ODRecordRef r;

	r = (ODRecordRef)CFArrayGetValueAtIndex(result, i);
	insert_share_record(shares, r);
    }

    safe_release(result);

    switch(Options::Command) {
    case Options::LIST_PENDING:
	return cmd_list_pending(shares);
    default:
	ASSERT(Options::Command == Options::SYNC);
	return cmd_sync_shares(shares);
    }

    return EX_OK;
}

/* vim: set cindent sw=4 ts=8 sts=4 tw=79 : */
