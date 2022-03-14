// Copyright (c) 2007-2008 Apple Inc. All Rights Reserved.
#import <stdint.h>
#import <pthread.h>

#import "ds_ops.h"
#import "Common.h"

#pragma mark constants in their DirectoryServices getup

static tDataListPtr _empty_data_list = NULL;
static tDataListPtr _default_attributes = NULL;
static tDataListPtr _user_record_type_list = NULL;
static tDataNodePtr _user_record_type = NULL;

static pthread_once_t ds_const_init = PTHREAD_ONCE_INIT; 
#define DS_CONST_INIT	pthread_once(&ds_const_init, initialize_directoryservice_statics);
void initialize_directoryservice_statics()
{
	_empty_data_list = dsDataListAllocate(0/*unused*/);

	// separate list for extractable attributes?  So far, that's only "_guest"
	_default_attributes = dsDataListAllocate(0/*unused*/);
	dsBuildListFromStringsAlloc(0/*unused*/, _default_attributes, 
		kDSNativeAttrTypePrefix "original_authentication_authority",
		kDSNativeAttrTypePrefix "home_info",
		kDSNativeAttrTypePrefix "_guest",
		kDSNAttrMetaNodeLocation, kDSNAttrRecordName, kDS1AttrGeneratedUID, 
		kDS1AttrUniqueID, kDS1AttrPrimaryGroupID,
		kDS1AttrDistinguishedName, kDS1AttrNFSHomeDirectory, kDSNAttrHomeDirectory,
		kDSNAttrAuthenticationAuthority, kDS1AttrAuthenticationHint, NULL);

	_user_record_type_list = dsDataListAllocate(0/*unused*/);
	dsAppendStringToListAlloc(0/*unused*/, _user_record_type_list, kDSStdRecordTypeUsers);

	_user_record_type = dsDataNodeAllocateString(/*unused*/0, kDSStdRecordTypeUsers);
}

tDataListPtr default_attributes() { DS_CONST_INIT; return _default_attributes; }
tDataListPtr user_record_type_list() { DS_CONST_INIT; return _user_record_type_list; }
tDataNodePtr user_record_type() { DS_CONST_INIT; return _user_record_type; }
tDataListPtr empty_data_list() { DS_CONST_INIT; return _empty_data_list; }

static inline tDataBufferPtr _get_data_buffer(tDirReference dir_ref, tDataBufferPtr in_buffer)
{
	int bufferSize = 1024;
	if (in_buffer) {
		bufferSize = 2 * in_buffer->fBufferSize;
		dsDataBufferDeAllocate(dir_ref, in_buffer);
		if (bufferSize > 1024*1024)
			return NULL;
	}
	return dsDataBufferAllocate(dir_ref, bufferSize);
}

tDataListPtr *
ds_find_dir_nodes(tDirReference dir_ref, tDirPatternMatch pattern, tDataListPtr _dataListPtr)
{
    tDirStatus status = eDSNoErr;
    UInt32 node_count;
    tContextData continuation_data_ptr = 0;
	tDataListPtr node_name_pattern_data_list_ptr = NULL;
	tDataBufferPtr buffer = NULL;
	tDataListPtr *results = NULL, *result = NULL;
	UInt32 result_count = 0;
	
	if (!_dataListPtr)
		node_name_pattern_data_list_ptr = empty_data_list();
	else
		node_name_pattern_data_list_ptr = _dataListPtr;

	buffer = _get_data_buffer(dir_ref, NULL);
	for (;;) {
		if (!buffer) break;
		status = dsFindDirNodes(dir_ref, buffer, 
							 node_name_pattern_data_list_ptr, pattern,
							 &node_count, &continuation_data_ptr);
		if (status == eDSBufferTooSmall) {
			buffer = _get_data_buffer(dir_ref, buffer);
			continue;
		}

		if (status != eDSNoErr)
			break;

		do {
			if (!node_count)
				break;

			results = reallocf(results, (result_count+node_count+1)*sizeof(tDataListPtr));
			result  = NULL;
			if (!results)
				break;
			
			result = &results[result_count];
			uint32_t node = node_count;
			while (node > 0) {
				status = dsGetDirNodeName(dir_ref, buffer, node, result);
				if (status == eDSNoErr)
					result++;
				node--;
			}
		} while (0);
		
		if (!continuation_data_ptr)
			break;
	}
	
	if (result)
		*result = NULL;
	
	if (continuation_data_ptr)
		dsReleaseContinueData(dir_ref, continuation_data_ptr);
	
	if (buffer)
		dsDataBufferDeAllocate(dir_ref, buffer);

	return results;
}

tDataListPtr
ds_get_node_for_user_record(tDirReference dir_ref, tDirNodeReference nodeRef, const char *name)
{
    tDirStatus status = eDSNoErr;
    UInt32 record_count = 1;
    tContextData continuation_data_ptr = 0;
	tDataListPtr node_location = NULL;
    tDataListPtr record_name_ptr = dsBuildListFromStrings(dir_ref, name, NULL);
    tDataListPtr attributes_ptr = dsBuildListFromStrings(dir_ref, kDSNAttrMetaNodeLocation, NULL);
	tDataBufferPtr buffer = _get_data_buffer(dir_ref, NULL);
	
	for (;;) {
		if (!buffer) break;
			
		status = dsGetRecordList(nodeRef, buffer,
			record_name_ptr, eDSExact,
			user_record_type_list(),
			attributes_ptr,
			false,
			&record_count,
			&continuation_data_ptr);
			
		if (status == eDSBufferTooSmall) {
			buffer = _get_data_buffer(dir_ref, buffer);
			continue;
		}

		if (status != eDSNoErr)
			break;
			
		do {
			if (!record_count)
				break;

			tAttributeListRef attr_list_ref = 0;
			tRecordEntryPtr record_entry_ptr = NULL; 
			
			status = dsGetRecordEntry(nodeRef, buffer,
				1, &attr_list_ref, &record_entry_ptr);
			if (status)
				break;

			unsigned int rec_attr_count = record_entry_ptr->fRecordAttributeCount;
			if (rec_attr_count != 1)
				break;
				
			tAttributeValueListRef attr_value_list_ref;
			tAttributeEntryPtr attr_info_ptr;
			if (dsGetAttributeEntry(nodeRef, buffer, attr_list_ref, 1,
									&attr_value_list_ref, &attr_info_ptr))
					break;
			unsigned long attr_value_count = attr_info_ptr->fAttributeValueCount;
			if (attr_value_count < 1)
				break;

			tAttributeValueEntryPtr value_entry_ptr;
			if (!dsGetAttributeValue( nodeRef, buffer, 1, attr_value_list_ref, &value_entry_ptr)) {
			    node_location = dsDataListAllocate(dir_ref);
				dsBuildListFromPathAlloc(dir_ref, node_location, value_entry_ptr->fAttributeValueData.fBufferData, "/");
			}
			dsDeallocAttributeEntry(dir_ref, attr_info_ptr);
			dsCloseAttributeValueList(attr_value_list_ref);
						
			dsCloseAttributeList(attr_list_ref);
			dsDeallocRecordEntry(dir_ref, record_entry_ptr);
		} while(0);
		
		if (!continuation_data_ptr)
			break;
	}
	dsDataListDeallocate(dir_ref, record_name_ptr);
	dsDataListDeallocate(dir_ref, attributes_ptr);

	if (continuation_data_ptr)
		dsReleaseContinueData(nodeRef, continuation_data_ptr);
		
	return node_location;
}


static CF_RETURNS_RETAINED
CFMutableDictionaryRef
ds_get_attributes(tDirReference dir_ref, tDirNodeReference nodeRef, tDataBufferPtr buffer, tRecordEntryPtr record_entry_ptr, tAttributeListRef attr_list_ref)
{
	unsigned int rec_attr_count = record_entry_ptr->fRecordAttributeCount;
	
	CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, rec_attr_count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!attributes)
		return NULL;
	unsigned int i;
    for ( i=1; i<= rec_attr_count; i++)
    {
        tAttributeValueListRef attr_value_list_ref;
        tAttributeEntryPtr attr_info_ptr;
        if (dsGetAttributeEntry( nodeRef, buffer, attr_list_ref, i,
                             &attr_value_list_ref, &attr_info_ptr ))
			return NULL;
        unsigned long attr_value_count = attr_info_ptr->fAttributeValueCount;
        if (attr_value_count > 0) {
			CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, attr_info_ptr->fAttributeSignature.fBufferData, kCFStringEncodingUTF8);
			CFTypeRef values = NULL;
			tAttributeValueEntryPtr value_entry_ptr;
			if (attr_value_count > 1)
				values = CFArrayCreateMutable(kCFAllocatorDefault, attr_value_count, &kCFTypeArrayCallBacks);
			unsigned int j;
			for (j=1; j <= attr_value_count; j++) {
				if (!dsGetAttributeValue( nodeRef, buffer, j, attr_value_list_ref, &value_entry_ptr)) {
					CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault, value_entry_ptr->fAttributeValueData.fBufferData, kCFStringEncodingUTF8);
					if (value) {
						if (attr_value_count > 1) {
							CFArrayAppendValue((CFMutableArrayRef)values, value);
							CFRelease(value);
						} else
							values = value;
					}
				}
			}
			if (key && values)
				CFDictionarySetValue(attributes, key, values);
            CFReleaseSafe(key);
            CFReleaseSafe(values);
        }
        dsDeallocAttributeEntry(dir_ref, attr_info_ptr);
        dsCloseAttributeValueList(attr_value_list_ref);
    }
	return attributes;
}

CF_RETURNS_RETAINED
CFMutableArrayRef
ds_get_user_records_for_name(tDirReference dir_ref, tDirNodeReference node_ref, const char *name, uint32_t max_records)
{
    tDirStatus status = eDSNoErr;
    uint32_t record_count = max_records; // This is an input variable too: stop after 1, which is much cheaper when using NetInfo
    tContextData continuation_data_ptr = 0;

    tDataListPtr record_name_ptr = dsBuildListFromStrings(dir_ref, name , NULL);
	tDataBufferPtr buffer = NULL;	
	CFMutableArrayRef results = NULL;
	
	buffer = _get_data_buffer(dir_ref, NULL);
	for (;;) {
		if (!buffer) break;
			
		status = dsGetRecordList(node_ref, buffer,
			record_name_ptr, eDSExact,
			user_record_type_list(),
			default_attributes(),
			false,
			&record_count,
			&continuation_data_ptr);
			
		if (status == eDSBufferTooSmall) {
			buffer = _get_data_buffer(dir_ref, buffer);
			continue;
		}
		else
			printf("buffer size: %u length: %u\n", 
				(uint32_t)buffer->fBufferSize, (uint32_t)buffer->fBufferLength);

		if (status != eDSNoErr)
			break;
			
		do {
			if (!record_count)
				break;

			if (!results)
				results = CFArrayCreateMutable(kCFAllocatorDefault, record_count, &kCFTypeArrayCallBacks);
			
			unsigned int i;
			for (i=1; i <= record_count; i++) {
				tAttributeListRef attr_list_ref = 0;
				tRecordEntryPtr record_entry_ptr = NULL; 
				
				status = dsGetRecordEntry(node_ref, buffer,
					i, &attr_list_ref, &record_entry_ptr);
				if (status)
					continue;
									
				CFMutableDictionaryRef attrs = ds_get_attributes(dir_ref, node_ref, buffer, record_entry_ptr, attr_list_ref);
				if (attrs) {
					CFArrayAppendValue(results, attrs);
					CFRelease(attrs);
				}

				dsCloseAttributeList(attr_list_ref);
				dsDeallocRecordEntry(dir_ref, record_entry_ptr);
			}
		} while(0);
		
		if (!continuation_data_ptr)
			break;
	}
	dsDataListDeallocate(dir_ref, record_name_ptr);

	if (continuation_data_ptr)
		dsReleaseContinueData(node_ref, continuation_data_ptr);
		
	return results;
}

tDirStatus 
ds_dir_node_auth_operation(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *operation, const char *username, const char *password, bool authentication_only)
{
	tDirStatus authStatus = eDSNotAuthorized;

    if (!username)
		return eDSUserUnknown;

    size_t ulNameLen = strlen(username);
    size_t ulPassLen = password ? strlen(password) : 0;
    tDataNodePtr _authType = dsDataNodeAllocateString (_directoryRef, operation); // dnAuthType
    tDataBufferPtr _authdataBufferPtr = dsDataBufferAllocate(_directoryRef,(uint32_t)(2 * sizeof (ulNameLen) + ulNameLen + ulPassLen)); // dbAuth
    tDataBufferPtr _stepBufferPtr = dsDataBufferAllocate(_directoryRef, 2048); // dbStep
	if (eDSNoErr != (authStatus = dsFillAuthBuffer(_authdataBufferPtr, 2, (uint32_t)ulNameLen, username, ulPassLen, password)))
		return authStatus;

    // Authenticate the user.
    authStatus = dsDoDirNodeAuth (_nodeRef, _authType, authentication_only, _authdataBufferPtr, _stepBufferPtr, 0);
    
	bzero(_authdataBufferPtr->fBufferData, _authdataBufferPtr->fBufferLength);
    dsDataNodeDeAllocate(_directoryRef, _authType);
    dsDataBufferDeAllocate(_directoryRef, _authdataBufferPtr);
    dsDataBufferDeAllocate(_directoryRef, _stepBufferPtr);

	return authStatus;
}


//[authNode setAttributeForUserRecord:[[userRecord name] UTF8String] length:[passwordHintData length] data:[passwordHintData bytes] type:kDS1AttrAuthenticationHint atIndex:1];

tDirStatus 
ds_set_attribute_in_user_record(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *recordname, size_t length, const void *data, const char *attr_type, uint32_t value_index)
{
	tDirStatus status = noErr;
    tDataNodePtr attributeType = dsDataNodeAllocateString(_directoryRef, attr_type);
	tDataNodePtr userRecordName = dsDataNodeAllocateString(_directoryRef, recordname);
	tRecordReference recordRef = 0;

	if (attributeType && userRecordName) do {
		status = dsOpenRecord(_nodeRef, user_record_type(), userRecordName, &recordRef);

		if (status)
			break;
			
		tAttributeValueEntryPtr valueEntryPtr = NULL;
		status = dsGetRecordAttributeValueByIndex(recordRef, attributeType, value_index, &valueEntryPtr);

		if (status) {
			// if getting the nth value failed, just add it
			tDataNodePtr attributeValue = dsDataNodeAllocateBlock(_directoryRef, (uint32_t)length, (uint32_t)length, (void*)data);
			if (attributeValue) {
				status = dsAddAttributeValue(recordRef, attributeType, attributeValue);
				dsDataNodeDeAllocate(_directoryRef, attributeValue);
			}
		} else {
			tAttributeValueEntryPtr attributeValue = dsAllocAttributeValueEntry(_directoryRef, valueEntryPtr->fAttributeValueID, (void*)data, (uint32_t)length);
			if (attributeValue) {
				status = dsSetAttributeValue(recordRef, attributeType, attributeValue);
				dsDeallocAttributeValueEntry(_directoryRef, attributeValue);
			}
			dsDeallocAttributeValueEntry(_directoryRef, valueEntryPtr);
		}
		dsCloseRecord(recordRef);
	} while (0);
	if (attributeType) dsDataNodeDeAllocate(_directoryRef, attributeType);
	if (userRecordName) dsDataNodeDeAllocate(_directoryRef, userRecordName);
	return status;
}


CF_RETURNS_RETAINED CFMutableArrayRef
ds_find_user_records_by_dsattr(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *attr, const char *value)
{ 
	// attr is e.g. kDSNAttrAuthenticationAuthority
	tDirStatus status = 0;
    UInt32 record_count = 0; // This is an input variable too
    tContextData continuation_data_ptr = 0;
    CFMutableArrayRef results = NULL;

	tDataNodePtr attributeType = dsDataNodeAllocateString(_directoryRef, attr);
	tDataNodePtr attributeValue = dsDataNodeAllocateString(_directoryRef, value);

	tDataBufferPtr buffer = NULL;	
	buffer = _get_data_buffer(_directoryRef, NULL);
	for (;;) {
		if (!buffer) break;			
			// should be eDSStartsWith such that we can 
			// find all entries and not key off opt parameters
			status = dsDoAttributeValueSearchWithData(
				_nodeRef, buffer,
				user_record_type_list(),
				attributeType, 
				eDSiExact,
				attributeValue,
				default_attributes(),	/* all attributes (NULL is documented to be legal but that's a documentation bug */
				false,			/* attr info and data */
				&record_count,
				&continuation_data_ptr);
			
		if (status == eDSBufferTooSmall) {
			buffer = _get_data_buffer(_directoryRef, buffer);
			continue;
		}

		if (status != eDSNoErr)
			break;
			
		do {
			if (!record_count)
				break;

			if (!results)
				results = CFArrayCreateMutable(kCFAllocatorDefault, record_count, &kCFTypeArrayCallBacks);
			
			unsigned int i;
			for (i=1; i <= record_count; i++) {
				tAttributeListRef attr_list_ref = 0;
				tRecordEntryPtr record_entry_ptr = NULL; 
				
				status = dsGetRecordEntry(_nodeRef, buffer,
					i, &attr_list_ref, &record_entry_ptr);
				if (status)
					continue;

				CFMutableDictionaryRef attrs = ds_get_attributes(_directoryRef, _nodeRef, buffer, record_entry_ptr, attr_list_ref);
				if (attrs) {
					CFArrayAppendValue(results, attrs);
					CFRelease(attrs);
				}

				dsCloseAttributeList(attr_list_ref);
				dsDeallocRecordEntry(_directoryRef, record_entry_ptr);
			}
		} while(0);
		
		if (!continuation_data_ptr)
			break;
	}

	if (continuation_data_ptr) dsReleaseContinueData(_nodeRef, continuation_data_ptr);
	if (attributeType) dsDataNodeDeAllocate(_directoryRef, attributeType);
	if (attributeValue) dsDataNodeDeAllocate(_directoryRef, attributeValue);

	return results;
}

bool 
ds_open_search_node(tDirReference _directoryRef, tDirNodeReference *_nodeRef)
{
	bool result = false;
	tDataListPtr *nodes = ds_find_dir_nodes(_directoryRef, eDSSearchNodeName, NULL);
	tDataListPtr node = NULL;
	if (nodes) {
		node = *nodes;
		free(nodes);
	}
	if (node) {
		result = !dsOpenDirNode(_directoryRef, node, _nodeRef);
		dsDataListDeallocate(_directoryRef, node);
	}
	return result;
}

bool
ds_open_node_for_user_record(tDirReference _directoryRef, const char *name, tDirNodeReference *_nodeRef)
{
	bool result = false;
	tDirNodeReference _searchnodeRef;
	if (ds_open_search_node(_directoryRef, &_searchnodeRef)) {
		tDataListPtr node_name_ptr = ds_get_node_for_user_record(_directoryRef, _searchnodeRef, name);
		dsCloseDirNode(_searchnodeRef);
		if (node_name_ptr) {
			result = !dsOpenDirNode(_directoryRef, node_name_ptr, _nodeRef);
			dsDataListDeallocate(_directoryRef, node_name_ptr);
		}
	}
	return result;
}

CF_RETURNS_RETAINED
CFMutableArrayRef find_user_record_by_attr_value(const char *attr, const char *value)
{
    tDirReference directory_ref = 0;
    tDirNodeReference node_ref = 0;
    CFMutableArrayRef results = NULL;
        
    do {
        if (dsOpenDirService(&directory_ref) != eDSNoErr)
            break;

        if (!ds_open_search_node(directory_ref, &node_ref))
            break;
                
        results = ds_find_user_records_by_dsattr(directory_ref, node_ref, attr, value);
        
        dsCloseDirNode(node_ref);
    } while (0);

    if (directory_ref)
        dsCloseDirService(directory_ref);
    return results;
}
