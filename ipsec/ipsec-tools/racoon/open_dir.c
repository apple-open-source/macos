/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "vmbuf.h"
#include "remoteconf.h"
#include "plog.h"
#include "misc.h"
#include "gcmalloc.h"
#include "open_dir.h"

#define BUF_LEN 		1024


static tDirStatus open_dir_get_search_node_ref (tDirReference dirRef, unsigned long index, 
                tDirNodeReference *searchNodeRef, unsigned long *count);
static tDirStatus open_dir_get_user_attr (tDirReference dirRef, tDirNodeReference searchNodeRef, char *user_name, 
                char *attr, tAttributeValueEntryPtr *attr_value);
static tDirStatus open_dir_check_group_membership (tDirReference dirRef, tDirNodeReference searchNodeRef, 
                char *group_name, char *user_name, char *userGID, int *authorized);


//----------------------------------------------------------------------
//	open_dir_authorize_id
//----------------------------------------------------------------------
int open_dir_authorize_id(vchar_t *id, vchar_t *group)
{

    tDirReference 			dirRef;
    tDirStatus 				dsResult = eDSNoErr;
    int						authorized = 0;
    tDirNodeReference 		searchNodeRef;
    tAttributeValueEntryPtr	groupID = NULL;
    tAttributeValueEntryPtr	recordName = NULL;
    unsigned long 			searchNodeCount;
    char*					user_name = NULL;
    char*					group_name = NULL;
    
    if (id == 0 || id->l < 1) {
        plog(ASL_LEVEL_ERR, "invalid user name.\n");
        goto end;
    }	
   	user_name = racoon_malloc(id->l + 1);
   	if (user_name == NULL) {
   		plog(ASL_LEVEL_ERR, "out of memory - unable to allocate space for user name.\n");
   		goto end;
   	}
   	bcopy(id->v, user_name, id->l);
   	*(user_name + id->l) = 0;
   		
   	if (group && group->l > 0) {
   		group_name = racoon_malloc(group->l + 1);
   		if (group_name == NULL) {
   			plog(ASL_LEVEL_ERR, "out of memeory - unable to allocate space for group name.\n");
   			goto end;
   		}
   		bcopy(group->v, group_name, group->l);
   		*(group_name + group->l) = 0;
   	}
   			
    if ((dsResult = dsOpenDirService(&dirRef)) == eDSNoErr) {  
        // get the search node ref
        if ((dsResult = open_dir_get_search_node_ref(dirRef, 1, &searchNodeRef, &searchNodeCount)) == eDSNoErr) {
            // get the user's primary group ID
           	if ((dsResult = open_dir_get_user_attr(dirRef, searchNodeRef, user_name, kDSNAttrRecordName, &recordName)) == eDSNoErr) {
           		if (recordName != 0) {
           			if (group_name != 0) {
						if ((dsResult = open_dir_get_user_attr(dirRef, searchNodeRef, user_name, kDS1AttrPrimaryGroupID, &groupID)) == eDSNoErr) {
							// check if user is member of the group
							dsResult = open_dir_check_group_membership(dirRef, searchNodeRef, group_name, 
								recordName->fAttributeValueData.fBufferData, groupID->fAttributeValueData.fBufferData, &authorized);
						}
					} else
						authorized = 1;	// no group required - user record found		
 				} 
            }
            if (groupID)
                dsDeallocAttributeValueEntry(dirRef, groupID);
            if (recordName)
            	dsDeallocAttributeValueEntry(dirRef, recordName);
            dsCloseDirNode(searchNodeRef);		// close the search node
        }
        dsCloseDirService(dirRef);
    }

end:    
    if (authorized)
        plog(ASL_LEVEL_NOTICE, "User '%s' authorized for access\n", user_name);
    else
        plog(ASL_LEVEL_NOTICE, "User '%s' not authorized for access\n", user_name);
    if (user_name)
    	free(user_name);
    if (group_name)
    	free(group_name);
    return authorized;
}


//----------------------------------------------------------------------
//	open_dir_get_search_node_ref
//----------------------------------------------------------------------
static tDirStatus open_dir_get_search_node_ref(tDirReference dirRef, unsigned long index, 
                tDirNodeReference *searchNodeRef, unsigned long *count)
{
    tDirStatus			dsResult = -1;
    tDataBufferPtr		searchNodeDataBufferPtr = 0;
    tDataListPtr	   	searchNodeNameDataListPtr = 0;

    unsigned long		outNodeCount;
    tContextData		continueData = 0;
    
    *searchNodeRef = 0;
    *count = 0;
    
    // allocate required buffers and data lists
    if ((searchNodeDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((searchNodeNameDataListPtr = dsDataListAllocate(dirRef)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
        
    // find authentication search node(s)
    if ((dsResult = dsFindDirNodes(dirRef, searchNodeDataBufferPtr, 0, eDSAuthenticationSearchNodeName, 
                                                                (UInt32*)&outNodeCount, &continueData)) == eDSNoErr) {
        if (outNodeCount != 0) {
                            
            // get the seach node name and open the node
            if ((dsResult = dsGetDirNodeName(dirRef, searchNodeDataBufferPtr, index, 
                                                        &searchNodeNameDataListPtr)) == eDSNoErr) {
                if ((dsResult = dsOpenDirNode(dirRef, searchNodeNameDataListPtr, searchNodeRef)) == eDSNoErr) {
                    *count = outNodeCount; 
                }
            }
        }
        if (continueData)
            dsReleaseContinueData(dirRef, continueData);
    }
    
cleanup:
    if (searchNodeDataBufferPtr)
        dsDataBufferDeAllocate(dirRef, searchNodeDataBufferPtr);
    if (searchNodeNameDataListPtr)
        dsDataListDeallocate(dirRef, searchNodeNameDataListPtr);
    
    return dsResult;
}

//----------------------------------------------------------------------
//	open_dir_get_user_attr
//----------------------------------------------------------------------
static tDirStatus open_dir_get_user_attr(tDirReference dirRef, tDirNodeReference searchNodeRef, char *user_name, 
                char *attr, tAttributeValueEntryPtr *attr_value)
{

    tDirStatus			dsResult = -1;
   
    tDataBufferPtr		userRcdDataBufferPtr = 0;
    tDataListPtr	   	recordNameDataListPtr = 0;
    tDataListPtr	   	recordTypeDataListPtr = 0;  
    tDataListPtr	   	attrTypeDataListPtr = 0;
    tContextData		continueData = 0;

    unsigned long		outRecordCount;
    int				userRcdFound = 0;
    u_int32_t			userRecordIndex, attrIndex;
    
    *attr_value	= 0;
                                             
    if ((userRcdDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((recordNameDataListPtr = dsBuildListFromStrings(dirRef, user_name, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((recordTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSStdRecordTypeUsers, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((attrTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSNAttrRecordName, kDS1AttrDistinguishedName, attr, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
                                                                 
    // find the user record(s), extracting the user name and requested attribute
    do {
        dsResult = dsGetRecordList(searchNodeRef, userRcdDataBufferPtr, recordNameDataListPtr, eDSExact,
                    recordTypeDataListPtr, attrTypeDataListPtr, 0, (UInt32*)&outRecordCount, &continueData);
        
        // if buffer too small - allocate a larger one
        if (dsResult == eDSBufferTooSmall) {
            u_int32_t	size = userRcdDataBufferPtr->fBufferSize * 2;
            
            dsDataBufferDeAllocate(dirRef, userRcdDataBufferPtr);
            if ((userRcdDataBufferPtr = dsDataBufferAllocate(dirRef, size)) == 0) {
                plog(ASL_LEVEL_ERR, "Could not allocate tDataBuffer\n");
		dsResult = -1;
		goto cleanup;
            }
        }
    } while (dsResult == eDSBufferTooSmall);

    if (dsResult == eDSNoErr) {
        // for each user record
        for (userRecordIndex = 1; (userRecordIndex <= outRecordCount) && (dsResult == eDSNoErr) 
                                                        && (userRcdFound == 0); userRecordIndex++) {
                            
            tAttributeListRef	attrListRef;
            tRecordEntryPtr	userRcdEntryPtr;
                
            // get the user record entry from the data buffer
            if ((dsResult = dsGetRecordEntry(searchNodeRef, userRcdDataBufferPtr, userRecordIndex, 
                                                    &attrListRef, &userRcdEntryPtr)) == eDSNoErr) {
                // for each attribute
                for (attrIndex = 1; (attrIndex <= userRcdEntryPtr->fRecordAttributeCount) 
                                                            && (dsResult == eDSNoErr); attrIndex++) {
                
                    tAttributeValueListRef	attrValueListRef;
                    tAttributeEntryPtr		attrInfoPtr;
                    tAttributeValueEntryPtr	attrValuePtr;
                
                    if ((dsResult = dsGetAttributeEntry(searchNodeRef, userRcdDataBufferPtr, 
                                        attrListRef, attrIndex, &attrValueListRef, &attrInfoPtr)) == eDSNoErr) {
                        if ((dsResult = dsGetAttributeValue(searchNodeRef, userRcdDataBufferPtr, 1, 
                                        attrValueListRef, &attrValuePtr)) == eDSNoErr) { 
                                
                            // check for user record name or attribute searching for
                            if (!strcmp(attrInfoPtr->fAttributeSignature.fBufferData, kDSNAttrRecordName)) {
                                if (!strcmp(attrValuePtr->fAttributeValueData.fBufferData, user_name)) 
                                    userRcdFound = 1;
                            }
                            if (!strcmp(attrInfoPtr->fAttributeSignature.fBufferData, kDS1AttrDistinguishedName)) {
                                if (!strcmp(attrValuePtr->fAttributeValueData.fBufferData, user_name)) 
                                    userRcdFound = 1;
                            }
                            if (!strcmp(attrInfoPtr->fAttributeSignature.fBufferData, attr)) {
                                *attr_value = attrValuePtr;	// return the attribute value
                                attrValuePtr = 0;		// set to zero so we don't deallocate it
                            }
                            if (attrValuePtr)
                                dsDeallocAttributeValueEntry(dirRef, attrValuePtr);
                        }
                        dsCloseAttributeValueList(attrValueListRef);
                        dsDeallocAttributeEntry(dirRef, attrInfoPtr);
                    }
                }
                // make sure we've processed both attributes and we have a match on user name
                if(userRcdFound == 0 || *attr_value == 0) {
                    userRcdFound = 0;
                    if (*attr_value)
                    	dsDeallocAttributeValueEntry(dirRef, *attr_value);
                    *attr_value = 0;
                }            
                dsCloseAttributeList(attrListRef);
                dsDeallocRecordEntry(dirRef, userRcdEntryPtr);
            }
        }
    }
        
cleanup:
	if (continueData)
		dsReleaseContinueData(searchNodeRef, continueData);
    if (userRcdDataBufferPtr)
        dsDataBufferDeAllocate(dirRef, userRcdDataBufferPtr);
    if (recordNameDataListPtr)
        dsDataListDeallocate(dirRef, recordNameDataListPtr);
    if (recordTypeDataListPtr)
        dsDataListDeallocate(dirRef, recordTypeDataListPtr); 
    if (attrTypeDataListPtr)
        dsDataListDeallocate(dirRef, attrTypeDataListPtr); 
        
    return dsResult;
    
}


//----------------------------------------------------------------------
//	open_dir_check_group_membership
//----------------------------------------------------------------------
static tDirStatus open_dir_check_group_membership(tDirReference dirRef, tDirNodeReference searchNodeRef, 
                char *group_name, char *user_name, char *userGID, int *authorized)
{
    tDirStatus			dsResult = -1;
   
    tDataBufferPtr		groupRcdDataBufferPtr = 0;
    tDataListPtr	   	recordNameDataListPtr = 0;
    tDataListPtr	   	recordTypeDataListPtr = 0;  
    tDataListPtr	   	attrTypeDataListPtr = 0;
    tContextData		continueData = 0;

    unsigned long		outRecordCount;
    u_int32_t			attrIndex, valueIndex;
    
    *authorized	= 0;
                                             
    if ((groupRcdDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((recordNameDataListPtr = dsBuildListFromStrings(dirRef, group_name, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((recordTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSStdRecordTypeGroups, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((attrTypeDataListPtr = dsBuildListFromStrings(dirRef, kDS1AttrPrimaryGroupID, kDSNAttrGroupMembership, 0)) == 0) {
        plog(ASL_LEVEL_ERR, "Could not allocate tDataList\n");
        goto cleanup;
    }

    // find the group record, extracting the group ID and group membership attribute
    do {
        dsResult = dsGetRecordList(searchNodeRef, groupRcdDataBufferPtr, recordNameDataListPtr, eDSExact,
                            recordTypeDataListPtr, attrTypeDataListPtr, 0, (UInt32*)&outRecordCount, &continueData);
        // if buffer too small - allocate a larger one
        if (dsResult == eDSBufferTooSmall) {
            u_int32_t	size = groupRcdDataBufferPtr->fBufferSize * 2;
            
            dsDataBufferDeAllocate(dirRef, groupRcdDataBufferPtr);
            if ((groupRcdDataBufferPtr = dsDataBufferAllocate(dirRef, size)) == 0) {
                plog(ASL_LEVEL_ERR, "Could not allocate tDataBuffer\n");
                dsResult = -1;
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
    }
        
cleanup:
	if (continueData)
		dsReleaseContinueData(searchNodeRef, continueData);
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

