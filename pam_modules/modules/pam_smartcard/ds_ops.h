// Copyright (c) 2007 Apple Inc. All Rights Reserved.

#import <CoreFoundation/CoreFoundation.h>
#import <DirectoryService/DirectoryService.h>

// find user records by attribute

bool ds_open_search_node(tDirReference _directoryRef, tDirNodeReference *_nodeRef);

CF_RETURNS_RETAINED
CFMutableArrayRef ds_find_user_records_by_dsattr(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *attr, const char *value);

// authenticate user and get info

bool ds_open_node_for_user_record(tDirReference _directoryRef, const char *name, tDirNodeReference *_nodeRef);

tDirStatus ds_dir_node_auth_operation(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *operation, const char *username, const char *password, bool authentication_only);

CF_RETURNS_RETAINED
CFMutableArrayRef ds_get_user_records_for_name(tDirReference dir_ref, tDirNodeReference node_ref, const char *name, uint32_t max_records);

tDirStatus ds_set_attribute_in_user_record(tDirReference _directoryRef, tDirNodeReference _nodeRef, const char *recordname, size_t length, const void *data, const char *attr_type, uint32_t value_index);

CFMutableArrayRef find_user_record_by_attr_value(const char *attr, const char *value);
