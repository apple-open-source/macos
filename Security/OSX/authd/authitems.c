/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "authitems.h"
#include "crc.h"
#include "debugging.h"

#include "authutilities.h"
#include <Security/AuthorizationTags.h>
#include <dispatch/private.h>

typedef struct _auth_item_s * auth_item_t;

#pragma mark -
#pragma mark auth_item_t

struct _auth_item_s {
    __AUTH_BASE_STRUCT_HEADER__;
	
    AuthorizationItem data;
    uint32_t type;
    size_t bufLen;
    
    CFStringRef cfKey;
};

static const char *
auth_item_get_string(auth_item_t item)
{
    if (item->bufLen <= item->data.valueLength) {
        item->bufLen = item->data.valueLength+1; // make sure buffer has a null char
        item->data.value = realloc(item->data.value, item->bufLen);
        if (item->data.value == NULL) {
            // this is added to prevent running off into random memory if a string buffer doesn't have a null char
            LOGE("realloc failed");
            abort();
        }
        ((uint8_t*)item->data.value)[item->bufLen-1] = '\0';
    }
    return item->data.value;
}

static CFStringRef
auth_item_get_cf_key(auth_item_t item)
{
    if (!item->cfKey) {
        item->cfKey = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, item->data.name, kCFStringEncodingUTF8, kCFAllocatorNull);
    }
    return item->cfKey;
}

static AuthorizationItem *
auth_item_get_auth_item(auth_item_t item)
{
    return &item->data;
}

static xpc_object_t
auth_item_copy_auth_item_xpc(auth_item_t item)
{
    xpc_object_t xpc_data = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(xpc_data, AUTH_XPC_ITEM_NAME, item->data.name);
    if (item->data.value) {
        // <rdar://problem/13033889> authd is holding on to multiple copies of my password in the clear
        bool sensitive = strcmp(item->data.name, "password") == 0;
        if (sensitive) {
            vm_address_t vmBytes = 0;
            size_t xpcOutOfBandBlockSize = (item->data.valueLength > 32768 ? item->data.valueLength : 32768); // min 16K on 64-bit systems and 12K on 32-bit systems
            vm_allocate(mach_task_self(), &vmBytes, xpcOutOfBandBlockSize, VM_FLAGS_ANYWHERE);
            memcpy((void *)vmBytes, item->data.value, item->data.valueLength);
            dispatch_data_t dispData = dispatch_data_create((void *)vmBytes, xpcOutOfBandBlockSize, DISPATCH_TARGET_QUEUE_DEFAULT, DISPATCH_DATA_DESTRUCTOR_VM_DEALLOCATE); // out-of-band mapping
            xpc_object_t xpcData = xpc_data_create_with_dispatch_data(dispData);
            dispatch_release(dispData);
            xpc_dictionary_set_value(xpc_data, AUTH_XPC_ITEM_VALUE, xpcData);
            xpc_release(xpcData);
            xpc_dictionary_set_uint64(xpc_data, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH, item->data.valueLength);
        } else {
            xpc_dictionary_set_data(xpc_data, AUTH_XPC_ITEM_VALUE, item->data.value, item->data.valueLength);
        }
    }
    xpc_dictionary_set_uint64(xpc_data, AUTH_XPC_ITEM_FLAGS, item->data.flags);
    xpc_dictionary_set_uint64(xpc_data, AUTH_XPC_ITEM_TYPE, item->type);
    return xpc_data;
}

static void
_auth_item_finalize(CFTypeRef value)
{
    auth_item_t item = (auth_item_t)value;
    
    CFReleaseSafe(item->cfKey);
    
    if (item->data.name) {
        free((void*)item->data.name);
    }

    if (item->data.value) {
        memset(item->data.value, 0, item->data.valueLength);
        free(item->data.value);
    }
}

static Boolean
_auth_item_equal(CFTypeRef value1, CFTypeRef value2)
{
    return (CFHash(value1) == CFHash(value2));
}

static CFStringRef
_auth_item_copy_description(CFTypeRef value)
{
    bool hidden = false;
    auth_item_t item = (auth_item_t)value;

#ifndef DEBUG
    static size_t passLen = strlen(kAuthorizationEnvironmentPassword);
    if (strncasecmp(item->data.name, kAuthorizationEnvironmentPassword, passLen) == 0) {
        hidden = true;
    }
#endif
    
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(desc, NULL, CFSTR("auth_item: %s, type=%i, length=%li, flags=%x"),
                        item->data.name, item->type,
                        hidden ? 0 : item->data.valueLength, (unsigned int)item->data.flags);
    
    switch (item->type) {
        case AI_TYPE_STRING:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%s"), hidden ? "(hidden)" : auth_item_get_string(item));
            break;
        case AI_TYPE_INT:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%i"), *(int32_t*)item->data.value);
            break;
        case AI_TYPE_UINT:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%u"), *(uint32_t*)item->data.value);
            break;
        case AI_TYPE_INT64:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%lli"), *(int64_t*)item->data.value);
            break;
        case AI_TYPE_UINT64:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%llu"), *(uint64_t*)item->data.value);
            break;
        case AI_TYPE_BOOL:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%i"), *(bool*)item->data.value);
            break;
        case AI_TYPE_DOUBLE:
            CFStringAppendFormat(desc, NULL, CFSTR(" value=%f"), *(double*)item->data.value);
            break;
        case AI_TYPE_DATA:
        case AI_TYPE_UNKNOWN:
            if (hidden) {
                CFStringAppendFormat(desc, NULL, CFSTR(" value=(hidden)"));
            } else {
                CFStringAppendFormat(desc, NULL, CFSTR(" value=0x"));
                size_t i = item->data.valueLength < 10 ? item->data.valueLength : 10;
                uint8_t * data = item->data.value;
                for (; i > 0; i--) {
                    CFStringAppendFormat(desc, NULL, CFSTR("%02x"), data[i-1]);
                }
            }
            break;
        default:
            break;
    }
    return desc;
}

static CFHashCode
_auth_item_hash(CFTypeRef value)
{
    auth_item_t item = (auth_item_t)value;
    uint64_t crc = crc64_init();
    crc = crc64_update(crc, item->data.name, strlen(item->data.name));
    if (item->data.value) {
        crc = crc64_update(crc, item->data.value, item->data.valueLength);
    }
    crc = crc64_update(crc, &item->data.flags, sizeof(item->data.flags));

    crc = crc64_final(crc);
    return (CFHashCode)crc;
}

AUTH_TYPE_INSTANCE(auth_item,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _auth_item_finalize,
                   .equal = _auth_item_equal,
                   .hash = _auth_item_hash,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _auth_item_copy_description
                   );

static CFTypeID auth_item_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_auth_item);
    });
    
    return type_id;
}

static auth_item_t
_auth_item_create()
{
    auth_item_t item = NULL;
    
    item = (auth_item_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, auth_item_get_type_id(), AUTH_CLASS_SIZE(auth_item), NULL);
    require(item != NULL, done);
    
done:
    return item;
}

static auth_item_t
auth_item_create(uint32_t type, const char * name, const void * value, size_t valueLen, uint32_t flags)
{
    auth_item_t item = NULL;
    require(name != NULL, done);
    
    item = _auth_item_create();
    require(item != NULL, done);
    
    item->type = type;
    item->data.flags = flags;
    item->data.name = _copy_string(name);
    item->data.valueLength = valueLen;
    item->bufLen = valueLen;
    if (value) {
        if (item->type == AI_TYPE_STRING) {
            item->bufLen++;
            item->data.value = calloc(1u, item->bufLen);
        } else if (valueLen) {
            item->data.value = calloc(1u, item->bufLen);
        }
        if (valueLen) {
            memcpy(item->data.value, value, valueLen);
        }
    }
    
done:
    return item;
}

static auth_item_t
auth_item_create_with_xpc(xpc_object_t data)
{
    auth_item_t item = NULL;
    require(data != NULL, done);
    require(xpc_get_type(data) == XPC_TYPE_DICTIONARY, done);
    require(xpc_dictionary_get_string(data, AUTH_XPC_ITEM_NAME) != NULL, done);
    
    item = _auth_item_create();
    require(item != NULL, done);
    
    item->data.name = _copy_string(xpc_dictionary_get_string(data, AUTH_XPC_ITEM_NAME));
    item->data.flags = (uint32_t)xpc_dictionary_get_uint64(data, AUTH_XPC_ITEM_FLAGS);
    item->type = (uint32_t)xpc_dictionary_get_uint64(data, AUTH_XPC_ITEM_TYPE);

    size_t len;
    const void * value = xpc_dictionary_get_data(data, AUTH_XPC_ITEM_VALUE, &len);
    if (value) {
        // <rdar://problem/13033889> authd is holding on to multiple copies of my password in the clear
        bool sensitive = xpc_dictionary_get_value(data, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH);
        if (sensitive) {
            size_t sensitiveLength = (size_t)xpc_dictionary_get_uint64(data, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH);
            item->bufLen = sensitiveLength;
            item->data.valueLength = sensitiveLength;
            item->data.value = calloc(1u, sensitiveLength);
            memcpy(item->data.value, value, sensitiveLength);
            memset_s((void *)value, len, 0, sensitiveLength); // clear the sensitive data, memset_s is never optimized away
        } else {
            item->bufLen = len;
            item->data.valueLength = len;
            item->data.value = calloc(1u, len);
            memcpy(item->data.value, value, len);
        }
    }
    
done:
    return item;
}

#pragma mark -
#pragma mark auth_items_t

struct _auth_items_s {
    __AUTH_BASE_STRUCT_HEADER__;

    CFMutableDictionaryRef dictionary;
    AuthorizationItemSet set;
};

static void
_auth_items_finalize(CFTypeRef value)
{
    auth_items_t items = (auth_items_t)value;

    CFReleaseNull(items->dictionary);
    free_safe(items->set.items)
}

static Boolean
_auth_items_equal(CFTypeRef value1, CFTypeRef value2)
{
    auth_items_t items1 = (auth_items_t)value1;
    auth_items_t items2 = (auth_items_t)value2;

    return CFEqual(items1->dictionary, items2->dictionary);
}

static CFStringRef
_auth_items_copy_description(CFTypeRef value)
{
    auth_items_t items = (auth_items_t)value;
    return CFCopyDescription(items->dictionary);
}

AUTH_TYPE_INSTANCE(auth_items,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _auth_items_finalize,
                   .equal = _auth_items_equal,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _auth_items_copy_description
                   );

CFTypeID auth_items_get_type_id()
{
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_auth_items);
    });
    
    return type_id;
}

static auth_items_t
_auth_items_create(bool createDict)
{
    auth_items_t items = NULL;
    
    items = (auth_items_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, auth_items_get_type_id(), AUTH_CLASS_SIZE(auth_items), NULL);
    require(items != NULL, done);
    
    if (createDict) {
        items->dictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
done:
    return items;
}

auth_items_t
auth_items_create()
{
    auth_items_t items = NULL;
    
    items = _auth_items_create(true);
    require(items != NULL, done);
    
done:
    return items;
}

static bool
_auth_items_parse_xpc(auth_items_t items, const xpc_object_t data)
{
    bool result = false;
    require(data != NULL, done);
    require(xpc_get_type(data) == XPC_TYPE_ARRAY, done);
    
    result = xpc_array_apply(data, ^bool(size_t index AUTH_UNUSED, xpc_object_t value) {
        
        auth_item_t item = auth_item_create_with_xpc(value);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
        
        return true;
    });
    
done:
    return result;
}

auth_items_t auth_items_create_with_xpc(const xpc_object_t data)
{
    auth_items_t items = NULL;

    items = _auth_items_create(true);
    require(items != NULL, done);
    
    _auth_items_parse_xpc(items, data);
    
done:
    return items;
}

auth_items_t
auth_items_create_copy(auth_items_t copy)
{
    auth_items_t items = NULL;
    
    items = _auth_items_create(false);
    require(items != NULL, done);
    
    items->dictionary = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(copy->dictionary), copy->dictionary);
    
done:
    return items;
}

size_t
auth_items_get_count(auth_items_t items)
{
    return (size_t)CFDictionaryGetCount(items->dictionary);
}

AuthorizationItemSet *
auth_items_get_item_set(auth_items_t items)
{
    uint32_t count = (uint32_t)CFDictionaryGetCount(items->dictionary);
    if (count) {
        size_t size = count * sizeof(AuthorizationItem);
        if (items->set.items == NULL) {
            items->set.items = calloc(1u, size);
            require(items->set.items != NULL, done);
        } else {
            if (count > items->set.count) {
                items->set.items = realloc(items->set.items, size);
                require_action(items->set.items != NULL, done, items->set.count = 0);
            }
        }
        items->set.count = count;
        CFTypeRef keys[count], values[count];
        CFDictionaryGetKeysAndValues(items->dictionary, keys, values);
        for (uint32_t i = 0; i < count; i++) {
            auth_item_t item = (auth_item_t)values[i];
            items->set.items[i] = *auth_item_get_auth_item(item);
        }
    } else {
        items->set.count = 0;
    }
    
done:
    return &items->set;
}

xpc_object_t
auth_items_export_xpc(auth_items_t items)
{
    xpc_object_t array = xpc_array_create(NULL, 0);
    
    _cf_dictionary_iterate(items->dictionary, ^bool(CFTypeRef key AUTH_UNUSED, CFTypeRef value) {
        auth_item_t item = (auth_item_t)value;
        xpc_object_t xpc_data = auth_item_copy_auth_item_xpc(item);
        xpc_array_append_value(array, xpc_data);
        xpc_release_safe(xpc_data);
        return true;
    });
    
    return array;
}

static auth_item_t
_find_item(auth_items_t items, const char * key)
{
    auth_item_t item = NULL;
    CFStringRef lookup = NULL;
    require(key != NULL, done);
    
    lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    require(lookup != NULL, done);
    
    item = (auth_item_t)CFDictionaryGetValue(items->dictionary, lookup);
    
done:
    CFReleaseSafe(lookup);
    return item;
}

void
auth_items_set_flags(auth_items_t items, const char *key, uint32_t flags)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
        item->data.flags |= flags;
    }
}

void
auth_items_clear_flags(auth_items_t items, const char *key, uint32_t flags)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
        item->data.flags &= ~flags;
    }
}

uint32_t
auth_items_get_flags(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
        return item->data.flags;
    }
    
    return 0;
}

bool
auth_items_check_flags(auth_items_t items, const char *key, uint32_t flags)
{
	// When several bits are set in uint32_t flags, "(current & flags) != 0" checks if ANY flag is set, not all flags!
	// This odd behavior is currently being relied upon in several places, so be careful when changing / fixing this.
	// However, this also risks unwanted information leakage in
	// AuthorizationCopyInfo ==> authorization_copy_info ==> [all info] auth_items_copy_with_flags
    uint32_t current = auth_items_get_flags(items,key);
    return flags ? (current & flags) != 0 : current == 0;
}

void
auth_items_set_key(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (!item) {
        item = auth_item_create(AI_TYPE_RIGHT, key, NULL, 0, 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

bool
auth_items_exist(auth_items_t items, const char *key)
{
    return _find_item(items,key) != NULL;
}

void
auth_items_remove(auth_items_t items, const char *key)
{
    CFStringRef lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFDictionaryRemoveValue(items->dictionary, lookup);
    CFReleaseSafe(lookup);
}

void
auth_items_remove_with_flags(auth_items_t items, uint32_t flags)
{
    auth_items_iterate(items, ^bool(const char *key) {
        if (auth_items_check_flags(items, key, flags)) {
            auth_items_remove(items,key);
        }
        return true;
    });
}

void
auth_items_clear(auth_items_t items)
{
    CFDictionaryRemoveAllValues(items->dictionary);
}

void
auth_items_copy(auth_items_t items, auth_items_t src)
{
    auth_items_iterate(src, ^bool(const char *key) {
        CFStringRef lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
        auth_item_t item = (auth_item_t)CFDictionaryGetValue(src->dictionary, lookup);
        CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
        CFReleaseSafe(lookup);
        return true;
    });
}

void
auth_items_copy_xpc(auth_items_t items, const xpc_object_t src)
{
    _auth_items_parse_xpc(items,src);
}

void
auth_items_copy_with_flags(auth_items_t items, auth_items_t src, uint32_t flags)
{
    auth_items_iterate(src, ^bool(const char *key) {
        if (auth_items_check_flags(src, key, flags)) {
            CFStringRef lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
            auth_item_t item = (auth_item_t)CFDictionaryGetValue(src->dictionary, lookup);
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(lookup);
        }
        return true;
    });
}

bool
auth_items_iterate(auth_items_t items, auth_items_iterator_t iter)
{
    bool result = false;
    CFTypeRef* keys = NULL;
    CFTypeRef* values = NULL;

    CFIndex count = CFDictionaryGetCount(items->dictionary);
    keys = calloc((size_t)count, sizeof(CFTypeRef));
    require(keys != NULL, done);
    
    values = calloc((size_t)count, sizeof(CFTypeRef));
    require(values != NULL, done);
    
    CFDictionaryGetKeysAndValues(items->dictionary, keys, values);
    for (CFIndex i = 0; i < count; i++) {
        auth_item_t item = (auth_item_t)values[i];
        result = iter(item->data.name);
        if (!result) {
            break;
        }
    }

done:
    free_safe(keys);
    free_safe(values);
    return result;
}

void
auth_items_set_string(auth_items_t items, const char *key, const char *value)
{
    assert(value); // marked non-null

    size_t valLen = strlen(value);
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_STRING && valLen < item->bufLen) {
        memcpy(item->data.value, value, valLen+1); // copy null
        item->data.valueLength = valLen;
    } else {
        item = auth_item_create(AI_TYPE_STRING, key, value, valLen, 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

const char *
auth_items_get_string(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type == AI_TYPE_STRING || item->type == AI_TYPE_UNKNOWN)) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i",
                 item->data.name, item->type, AI_TYPE_STRING);
        }
#endif
        return auth_item_get_string(item);
    }

    return NULL;
}

void
auth_items_set_data(auth_items_t items, const char *key, const void *value, size_t len)
{
    assert(value); // marked non-null

    if (len) {
        auth_item_t item = _find_item(items,key);
        if (item && item->type == AI_TYPE_DATA && len <= item->bufLen) {
            memcpy(item->data.value, value, len);
            item->data.valueLength = len;
        } else {
            item = auth_item_create(AI_TYPE_DATA, key, value, len, 0);
            if (item) {
                CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
                CFReleaseSafe(item);
            }
        }
    }
}

const void *
auth_items_get_data(auth_items_t items, const char *key, size_t *len)
{
    assert(len); // marked non-null

    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type == AI_TYPE_DATA || item->type == AI_TYPE_UNKNOWN)) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i",
                 item->data.name, item->type, AI_TYPE_DATA);
        }
#endif
        *len = item->data.valueLength;
        return item->data.value;
    }
    
    return NULL;
}

const void *
auth_items_get_data_with_flags(auth_items_t items, const char *key, size_t *len, uint32_t flags)
{
    assert(len); // marked non-null

	auth_item_t item = _find_item(items,key);
	if (item && (item->data.flags & flags) == flags) {
#if DEBUG
		if (!(item->type == AI_TYPE_DATA || item->type == AI_TYPE_UNKNOWN)) {
			LOGV("auth_items: key = %s, invalid type=%i expected=%i",
				 item->data.name, item->type, AI_TYPE_DATA);
		}
#endif
		*len = item->data.valueLength;

		return item->data.value;
	}

	return NULL;
}

void
auth_items_set_bool(auth_items_t items, const char *key, bool value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_BOOL) {
        *(bool*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_BOOL, key, &value, sizeof(bool), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

bool
auth_items_get_bool(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type == AI_TYPE_BOOL || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(bool))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_BOOL, item->data.valueLength, sizeof(bool));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return atoi(auth_item_get_string(item));
        }
        
        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(bool), done);
        
        return *(bool*)item->data.value;
    } 

done:
    return false;
}

void
auth_items_set_int(auth_items_t items, const char *key, int32_t value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_INT) {
        *(int32_t*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_INT, key, &value, sizeof(int32_t), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

int32_t
auth_items_get_int(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type ==AI_TYPE_INT || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(int32_t))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_INT, item->data.valueLength, sizeof(int32_t));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return atoi(auth_item_get_string(item));
        }
        
        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(int32_t), done);
        
        return *(int32_t*)item->data.value;
    }

done:
    return 0;
}

void
auth_items_set_uint(auth_items_t items, const char *key, uint32_t value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_UINT) {
        *(uint32_t*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_UINT, key, &value, sizeof(uint32_t), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

uint32_t
auth_items_get_uint(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type ==AI_TYPE_UINT || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(uint32_t))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_UINT, item->data.valueLength, sizeof(uint32_t));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return (uint32_t)atoi(auth_item_get_string(item));
        }
        
        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(uint32_t), done);
        
        return *(uint32_t*)item->data.value;
    }
    
done:
    return 0;
}

void
auth_items_set_int64(auth_items_t items, const char *key, int64_t value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_INT64) {
        *(int64_t*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_INT64, key, &value, sizeof(int64_t), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

int64_t
auth_items_get_int64(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type ==AI_TYPE_INT64 || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(int64_t))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_INT64, item->data.valueLength, sizeof(int64_t));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return atoll(auth_item_get_string(item));
        }

        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(int64_t), done);
        
        return *(int64_t*)item->data.value;
    }

done:
    return 0;
}

void
auth_items_set_uint64(auth_items_t items, const char *key, uint64_t value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_UINT64) {
        *(uint64_t*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_UINT64, key, &value, sizeof(uint64_t), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

uint64_t
auth_items_get_uint64(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type ==AI_TYPE_UINT64 || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(uint64_t))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_UINT64, item->data.valueLength, sizeof(uint64_t));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return (uint64_t)atoll(auth_item_get_string(item));
        }
        
        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(uint64_t), done);
        
        return *(uint64_t*)item->data.value;
    }

done:
    return 0;
}

void auth_items_set_double(auth_items_t items, const char *key, double value)
{
    auth_item_t item = _find_item(items,key);
    if (item && item->type == AI_TYPE_DOUBLE) {
        *(double*)item->data.value = value;
    } else {
        item = auth_item_create(AI_TYPE_DOUBLE, key, &value, sizeof(double), 0);
        if (item) {
            CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
            CFReleaseSafe(item);
        }
    }
}

double auth_items_get_double(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
#if DEBUG
        if (!(item->type ==AI_TYPE_DOUBLE || item->type == AI_TYPE_UNKNOWN) || (item->data.valueLength != sizeof(double))) {
            LOGV("auth_items: key = %s, invalid type=%i expected=%i or size=%li expected=%li",
                 item->data.name, item->type, AI_TYPE_DOUBLE, item->data.valueLength, sizeof(double));
        }
#endif
        if (item->type == AI_TYPE_STRING) {
            return atof(auth_item_get_string(item));
        }
        
        require(item->data.value != NULL, done);
        require(item->data.valueLength == sizeof(double), done);
        
        return *(double*)item->data.value;
    }
    
done:
    return 0;
}

uint32_t auth_items_get_type(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
        return item->type;
    }

    return AI_TYPE_UNKNOWN;
}

size_t auth_items_get_length(auth_items_t items, const char *key)
{
    auth_item_t item = _find_item(items,key);
    if (item) {
        return item->data.valueLength;
    }
    
    return 0;
}

void auth_items_set_value(auth_items_t items, const char *key, uint32_t type, uint32_t flags, const void *value, size_t len)
{
    auth_item_t item = auth_item_create(type, key, value, len, flags);
    if (item) {
        CFDictionarySetValue(items->dictionary, auth_item_get_cf_key(item), item);
        CFReleaseSafe(item);
    }
}

#pragma mark -
#pragma mark auth_rights_t

struct _auth_rights_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    CFMutableArrayRef array;
};

static void
_auth_rights_finalize(CFTypeRef value)
{
    auth_rights_t rights = (auth_rights_t)value;
    
    CFReleaseNull(rights->array);
}

static Boolean
_auth_rights_equal(CFTypeRef value1, CFTypeRef value2)
{
    auth_rights_t rights1 = (auth_rights_t)value1;
    auth_rights_t rights2 = (auth_rights_t)value2;
    
    return CFEqual(rights1->array, rights2->array);
}

static CFStringRef
_auth_rights_copy_description(CFTypeRef value)
{
    auth_rights_t rights = (auth_rights_t)value;
    return CFCopyDescription(rights->array);
}

AUTH_TYPE_INSTANCE(auth_rights,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _auth_rights_finalize,
                   .equal = _auth_rights_equal,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _auth_rights_copy_description
                   );

static CFTypeID auth_rights_get_type_id()
{
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_auth_rights);
    });
    
    return type_id;
}

static auth_rights_t
_auth_rights_create()
{
    auth_rights_t rights = NULL;
    
    rights = (auth_rights_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, auth_rights_get_type_id(), AUTH_CLASS_SIZE(auth_rights), NULL);
    require(rights != NULL, done);
    
    rights->array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
done:
    return rights;
}

auth_rights_t
auth_rights_create()
{
    auth_rights_t rights = _auth_rights_create();
    require(rights != NULL, done);
    
done:
    return rights;
}

auth_rights_t auth_rights_create_with_xpc(const xpc_object_t data)
{
    auth_rights_t rights = _auth_rights_create();
    require(rights != NULL, done);
    require(data != NULL, done);
    require(xpc_get_type(data) == XPC_TYPE_ARRAY, done);
    
    xpc_array_apply(data, ^bool(size_t index AUTH_UNUSED, xpc_object_t value) {
        
        auth_item_t item = auth_item_create_with_xpc(value);
        if (item) {
            CFArrayAppendValue(rights->array, item);
            CFReleaseSafe(item);
        }
        
        return true;
    });
    
done:
    return rights;
}

xpc_object_t auth_rights_export_xpc(auth_rights_t rights)
{
    xpc_object_t array = xpc_array_create(NULL, 0);
    
    CFIndex count = CFArrayGetCount(rights->array);
    for (CFIndex i = 0; i < count; i++) {
        auth_item_t item = (auth_item_t)CFArrayGetValueAtIndex(rights->array, i);
        xpc_object_t xpc_data = auth_item_copy_auth_item_xpc(item);
        xpc_array_append_value(array, xpc_data);
        xpc_release_safe(xpc_data);
    }
    
    return array;
}

static auth_item_t
_find_right_item(auth_rights_t rights, const char * key)
{
    auth_item_t item = NULL;
    CFStringRef lookup = NULL;
    require(key != NULL, done);
    
    lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    require(lookup != NULL, done);
    
    CFIndex count = CFArrayGetCount(rights->array);
    for (CFIndex i = 0; i < count; i++) {
        auth_item_t tmp = (auth_item_t)CFArrayGetValueAtIndex(rights->array, i);
        if (tmp && CFEqual(auth_item_get_cf_key(tmp), lookup)) {
            item = tmp;
            break;
        }
    }
    
done:
    CFReleaseSafe(lookup);
    return item;
}

void auth_rights_set_flags(auth_rights_t rights, const char *key, uint32_t flags)
{
    auth_item_t item = _find_right_item(rights,key);
    if (item) {
        item->data.flags |= flags;
    }
}

void auth_rights_clear_flags(auth_rights_t rights, const char *key, uint32_t flags)
{
    auth_item_t item = _find_right_item(rights,key);
    if (item) {
        item->data.flags &= ~flags;
    }
}

uint32_t auth_rights_get_flags(auth_rights_t rights, const char *key)
{
    auth_item_t item = _find_right_item(rights,key);
    if (item) {
        return item->data.flags;
    }
    
    return 0;
}

bool auth_rights_check_flags(auth_rights_t rights, const char *key, uint32_t flags)
{
    uint32_t current = auth_rights_get_flags(rights,key);
    return flags ? (current & flags) != 0 : current == 0;
}

size_t auth_rights_get_count(auth_rights_t rights)
{
    return (size_t)CFArrayGetCount(rights->array);
}

void auth_rights_add(auth_rights_t rights, const char *key)
{
    auth_item_t item = auth_item_create(AI_TYPE_RIGHT, key, NULL, 0, 0);
    if (item) {
        CFArrayAppendValue(rights->array, item);
        CFReleaseSafe(item);
    }
}

bool auth_rights_exist(auth_rights_t rights, const char *key)
{
    return (_find_right_item(rights,key) != NULL);
}

void auth_rights_remove(auth_rights_t rights, const char *key)
{
    CFStringRef lookup = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFIndex count = CFArrayGetCount(rights->array);
    for (CFIndex i = 0; i < count; i++) {
        auth_item_t item = (auth_item_t)CFArrayGetValueAtIndex(rights->array, i);
        if (CFEqual(auth_item_get_cf_key(item), lookup)) {
            CFArrayRemoveValueAtIndex(rights->array, i);
            i--;
            count--;
        }
    }
    CFReleaseSafe(lookup);
}

void auth_rights_clear(auth_rights_t rights)
{
    CFArrayRemoveAllValues(rights->array);
}

bool
auth_rights_iterate(auth_rights_t rights, bool(^iter)(const char * key))
{
    bool result = false;
    
    CFIndex count = CFArrayGetCount(rights->array);
    for (CFIndex i = 0; i < count; i++) {
        auth_item_t item = (auth_item_t)CFArrayGetValueAtIndex(rights->array, i);
        result = iter(item->data.name);
        if (!result) {
            break;
        }
    }
    
    return result;
}
