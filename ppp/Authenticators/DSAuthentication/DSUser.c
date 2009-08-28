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

#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include "../../Helpers/pppd/pppd.h"

#include "DSUser.h"

#define BUF_LEN 1024

//----------------------------------------------------------------------
//	dsauth_get_search_node_ref
//----------------------------------------------------------------------
tDirStatus dsauth_get_search_node_ref(tDirReference dirRef, UInt32 index, 
                tDirNodeReference *searchNodeRef, UInt32 *count)
{
    tDirStatus			dsResult = -1;
    tDataBufferPtr		searchNodeDataBufferPtr = 0;
    tDataListPtr	   	searchNodeNameDataListPtr = 0;

    UInt32			outNodeCount;
    tContextData		continueData = 0;
    
    *searchNodeRef = 0;
    *count = 0;
    
    // allcoate required buffers and data lists
    if ((searchNodeDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        error("DS plugin: Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((searchNodeNameDataListPtr = dsDataListAllocate(dirRef)) == 0) {
        error("DS plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
        
    // find authentication search node(s)
    if ((dsResult = dsFindDirNodes(dirRef, searchNodeDataBufferPtr, 0, eDSAuthenticationSearchNodeName, 
                                                                &outNodeCount, &continueData)) == eDSNoErr) {
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
//	dsauth_get_user_attr
//----------------------------------------------------------------------
tDirStatus dsauth_get_user_attr(tDirReference dirRef, tDirNodeReference searchNodeRef, char *user_name, 
                char *attr, tAttributeValueEntryPtr *attr_value)
{

    tDirStatus			dsResult = -1;
   
    tDataBufferPtr		userRcdDataBufferPtr = 0;
    tDataListPtr	   	recordNameDataListPtr = 0;
    tDataListPtr	   	recordTypeDataListPtr = 0;  
    tDataListPtr	   	attrTypeDataListPtr = 0;
    tContextData		continueData = 0;

    UInt32				outRecordCount;
    int					userRcdFound = 0;
    UInt32				userRecordIndex, attrIndex;
    
    *attr_value	= 0;
                                             
    if ((userRcdDataBufferPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        error("DS plugin: Could not allocate tDataBuffer\n");
        goto cleanup;
    }
    if ((recordNameDataListPtr = dsBuildListFromStrings(dirRef, user_name, 0)) == 0) {
        error("DS plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((recordTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSStdRecordTypeUsers, 0)) == 0) {
        error("DS plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
    if ((attrTypeDataListPtr = dsBuildListFromStrings(dirRef, kDSNAttrRecordName, kDS1AttrDistinguishedName, attr, 0)) == 0) {
        error("DS plugin: Could not allocate tDataList\n");
        goto cleanup;
    }
                                                                 
    // find the user record(s), extracting the user name and requested attribute
    do {
        dsResult = dsGetRecordList(searchNodeRef, userRcdDataBufferPtr, recordNameDataListPtr, eDSExact,
                    recordTypeDataListPtr, attrTypeDataListPtr, 0, &outRecordCount, &continueData);
        
        // if buffer too small - allocate a larger one
        if (dsResult == eDSBufferTooSmall) {
            u_int32_t	size = userRcdDataBufferPtr->fBufferSize * 2;
            
            dsDataBufferDeAllocate(dirRef, userRcdDataBufferPtr);
            if ((userRcdDataBufferPtr = dsDataBufferAllocate(dirRef, size)) == 0) {
                error("DS plugin: Could not allcoate tDataBuffer\n");
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



