/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add support for authentication through Directory Services.
 *
----------------------------------------------------------------------------- */


#include <stdio.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../../Helpers/pppd/pppd.h"
#include "../../Helpers/pppd/pppd.h"
#include "../DSAuthentication/DSUser.h"
#include "../../Helpers/vpnd/RASSchemaDefinitions.h"

#define BUF_LEN 		1024
#define GROUP_NAME_BUF_SIZE	256

extern char		*serverid; 	// option defined in sys-MacOSX.c

static CFBundleRef 	bundle = 0;
static char		group_name[GROUP_NAME_BUF_SIZE] = "";	// from plist file
static char		*opt_group = 0;

static option_t Options[] = {
    { "dsaclgroup", o_string, &opt_group,
      "Authorization group", OPT_PRIV},
    { NULL }
};


static void dsaccess_phase_changed(void *arg, int phase);
static int dsaccess_authorize_user(char* user_name, int len);
static tDirStatus dsaccess_check_group_membership(tDirReference dirRef,
        tDirNodeReference searchNodeRef, char *group_name, char *user_name, char *userGID, int *authorized);

/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */

int start(CFBundleRef ref)
{

    bundle = ref;

    CFRetain(bundle);

    // setup hooks
    acl_hook = dsaccess_authorize_user;

    add_options(Options);
    
    add_notifier(&phasechange, dsaccess_phase_changed, 0);    // to setup authorization group name
        
    info("Directory Services Authorization plugin initialized\n");
        
    return 0;
}

//----------------------------------------------------------------------
//	dsaccess_phase_changed
//	
//	On first phase change:
//		get the group name option from the plist file from the 
//		DSACL dictionary
//----------------------------------------------------------------------
static void dsaccess_phase_changed(void *arg, int phase)
{

    SCPreferencesRef 	prefs;
    CFDictionaryRef	dict;
    CFPropertyListRef	serverRef; 
    CFStringRef		serverIDRef;
    CFStringRef		groupNameRef;
    CFPropertyListRef	servers_list;
      
    remove_notifier(&phasechange, dsaccess_phase_changed, 0);

    if (opt_group == 0) {    
        if (serverid == 0) {
            error("DSAccessControl plugin: Invalid server id\n");
            return;
        }
        /* open the prefs file */
        if ((prefs = SCPreferencesCreate(0, SCSTR("pppd"), kRASServerPrefsFileName)) != 0) {
            // get servers list from the plist
            if ((servers_list = SCPreferencesGetValue(prefs, kRASServers)) != 0) {
                /* retrieve the information for the given Server ID */ 
                if ((serverIDRef = CFStringCreateWithCString(0, serverid, kCFStringEncodingMacRoman)) != 0) {
                    if ((serverRef = CFDictionaryGetValue(servers_list, serverIDRef)) != 0) {
                        // get authorization group name
                        dict = CFDictionaryGetValue(serverRef, kRASEntDSACL);
                        if (dict && CFGetTypeID(dict) == CFDictionaryGetTypeID()) {
                            groupNameRef = CFDictionaryGetValue(dict, kRASPropDSACLGroup);
                            if (groupNameRef && CFGetTypeID(groupNameRef) == CFStringGetTypeID())
                                CFStringGetCString(groupNameRef, group_name, GROUP_NAME_BUF_SIZE, kCFStringEncodingMacRoman);
                        }
                    }
                    CFRelease(serverIDRef);
                }
            }
            CFRelease(prefs);
        }
    } else
       memcpy(group_name, opt_group, strlen(opt_group));
       
    if (*group_name == 0)
        error("DSAccessControl plugin: no authorization group name provided\n");
}


//----------------------------------------------------------------------
//	dsaccess_authorize_user
//----------------------------------------------------------------------
static int dsaccess_authorize_user(char* name, int len)
{

    tDirReference 		dirRef;
    tDirStatus 			dsResult = eDSNoErr;
    int				authorized = 0;
    tDirNodeReference 		searchNodeRef;
    tAttributeValueEntryPtr	groupID;
    tAttributeValueEntryPtr	recordName = 0;
    unsigned long 		searchNodeCount;
    char*			user_name;
    
    if (*group_name == 0) {	 // was a group name specified ?
        error("DSAccessControl plugin: no authorization group specified - users cannot be authorized\n");
        return 0;
    }
    if (len < 1) {
        error("DSAccessControl plugin: invalid user name has zero length\n");
        return 0;
    }
    if ((user_name = (char*)malloc(len + 1)) == 0) {
        error("DSAccessControl plugin: unable to allocate memory for user name\n");
        return 0;
    }
    bcopy(name, user_name, len);
    *(user_name + len) = 0;

    if ((dsResult = dsOpenDirService(&dirRef)) == eDSNoErr) {  
        // get the search node ref
        if ((dsResult = dsauth_get_search_node_ref(dirRef, 1, &searchNodeRef, &searchNodeCount)) == eDSNoErr) {
            // get the user's primary group ID
            if ((dsResult = dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDS1AttrPrimaryGroupID, &groupID)) == eDSNoErr) {
                dsResult = dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDSNAttrRecordName, &recordName);
                if (dsResult == eDSNoErr && groupID != 0 && recordName != 0) {
                    // check if user is member of the group
                    dsResult = dsaccess_check_group_membership(dirRef, searchNodeRef, group_name, 
                                        recordName->fAttributeValueData.fBufferData, 
                                        groupID->fAttributeValueData.fBufferData, &authorized);
                }
            }
            if (groupID)
                dsDeallocAttributeValueEntry(dirRef, groupID);
            dsCloseDirNode(searchNodeRef);		// close the search node
        }
        dsCloseDirService(dirRef);
    }

    
    if (authorized)
        info("DSAccessControl plugin: User '%s' authorized for access\n", user_name);
    else
        info("DSAccessControl plugin: User '%s' not authorized for access\n", user_name);
    free(user_name);
    return authorized;
}

//----------------------------------------------------------------------
//	dsaccess_check_group_membership
//----------------------------------------------------------------------
static tDirStatus dsaccess_check_group_membership(tDirReference dirRef, tDirNodeReference searchNodeRef, 
                char *group_name, char *user_name, char *userGID, int *authorized)
{
    tDirStatus			dsResult = eDSNoErr;
   
    tDataBufferPtr		groupRcdDataBufferPtr = 0;
    tDataListPtr	   	recordNameDataListPtr = 0;
    tDataListPtr	   	recordTypeDataListPtr = 0;  
    tDataListPtr	   	attrTypeDataListPtr = 0;
    tContextData		continueData = 0;

    unsigned long		outRecordCount;
    u_int32_t			attrIndex, valueIndex;
    
    *authorized	= 0;
                                             
    if ((groupRcdDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        error("DSAccessControl plugin: Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((recordNameDataListPtr = dsBuildListFromStrings(dirRef, group_name, 0)) == 0) {
        error("DSAccessControl plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((recordTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSStdRecordTypeGroups, 0)) == 0) {
        error("DSAccessControl plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((attrTypeDataListPtr = dsBuildListFromStrings(dirRef, kDS1AttrPrimaryGroupID, kDSNAttrGroupMembership, 0)) == 0) {
        error("DSAccessControl plugin: Could not allocate tDataList\n");
        goto cleanup;
    }

    // find the group record, extracting the group ID and group membership attribute
    do {
        dsResult = dsGetRecordList(searchNodeRef, groupRcdDataBufferPtr, recordNameDataListPtr, eDSExact,
                            recordTypeDataListPtr, attrTypeDataListPtr, 0, &outRecordCount, &continueData);
        // if buffer too small - allocate a larger one
        if (dsResult == eDSBufferTooSmall) {
            u_int32_t	size = groupRcdDataBufferPtr->fBufferSize * 2;
            
            dsDataBufferDeAllocate(dirRef, groupRcdDataBufferPtr);
            if ((groupRcdDataBufferPtr = dsDataBufferAllocate(dirRef, size)) == 0) {
                error("DS plugin: Could not allocate tDataBuffer\n");
                goto cleanup;
            }
        }
    } while (dsResult == eDSBufferTooSmall);
    
    if (dsResult == eDSNoErr) {
                             
        tAttributeListRef	attrListRef;
        tRecordEntryPtr		groupRcdEntryPtr;
        
        // get the group record entry
        if ((dsResult = dsGetRecordEntry(searchNodeRef, groupRcdDataBufferPtr, 1, &attrListRef, &groupRcdEntryPtr)) == eDSNoErr) {

            // for each attribute
            for (attrIndex = 1; (attrIndex <= groupRcdEntryPtr->fRecordAttributeCount) && (dsResult == eDSNoErr)
                                    && (*authorized == 0); attrIndex++) {
                    
                tAttributeValueListRef	attrValueListRef;
                tAttributeEntryPtr	attrInfoPtr;
                tAttributeValueEntryPtr	attrValuePtr;
            
                if ((dsResult = dsGetAttributeEntry(searchNodeRef, groupRcdDataBufferPtr, attrListRef, 
                                                        attrIndex, &attrValueListRef, &attrInfoPtr)) == eDSNoErr) {
                    
                    // group ID attribute ?
                    if (!strcmp(attrInfoPtr->fAttributeSignature.fBufferData, kDS1AttrPrimaryGroupID)) {
                    	if ((dsResult = dsGetAttributeValue(searchNodeRef, groupRcdDataBufferPtr, 1, 
                                                            attrValueListRef, &attrValuePtr)) == eDSNoErr) {  
                            
                            // check for match on primary group ID
                            if (!strcmp(attrValuePtr->fAttributeValueData.fBufferData, userGID))
                                *authorized = 1;
                            dsDeallocAttributeValueEntry(dirRef, attrValuePtr);
                        }
                    } else if (!strcmp(attrInfoPtr->fAttributeSignature.fBufferData, kDSNAttrGroupMembership)) {
                        // for each value check for user's name in the group
                        for (valueIndex = 1; (valueIndex <= attrInfoPtr->fAttributeValueCount) 
                                                && (dsResult == eDSNoErr) && (*authorized == 0); valueIndex++) {
                            
                            if ((dsResult = dsGetAttributeValue(searchNodeRef, groupRcdDataBufferPtr, 
                                                    valueIndex, attrValueListRef, &attrValuePtr)) == eDSNoErr) {                                
                                if (!strcmp(attrValuePtr->fAttributeValueData.fBufferData, user_name))
                                    *authorized = 1;
                                dsDeallocAttributeValueEntry(dirRef, attrValuePtr);
                            }
                        }
                    }
                    dsCloseAttributeValueList(attrValueListRef);
                    dsDeallocAttributeEntry(dirRef, attrInfoPtr);
                }
            }
            dsCloseAttributeList(attrListRef);
            dsDeallocRecordEntry(dirRef, groupRcdEntryPtr);
        }
        if (continueData)
            dsReleaseContinueData(dirRef, continueData);
    }
        
cleanup:
    if (groupRcdDataBufferPtr)
        dsDataBufferDeAllocate(dirRef, groupRcdDataBufferPtr);
    if (recordNameDataListPtr)
        dsDataListDeallocate(dirRef, recordNameDataListPtr);
    if (recordTypeDataListPtr)
        dsDataListDeallocate(dirRef, recordTypeDataListPtr); 
    if (attrTypeDataListPtr)
        dsDataListDeallocate(dirRef, attrTypeDataListPtr); 
        
    return dsResult;
}

