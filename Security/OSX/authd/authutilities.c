/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "authutilities.h"
#include "authd_private.h"
#include "debugging.h"

#include <AssertMacros.h>
#include <assert.h>
#include <syslog.h>

xpc_object_t
SerializeItemSet(const AuthorizationItemSet * itemSet)
{
    xpc_object_t set = NULL;
    require_quiet(itemSet != NULL, done);
    require_quiet(itemSet->count != 0, done);
    
    set = xpc_array_create(NULL, 0);
    require(set != NULL, done);
    
    for (uint32_t i = 0; i < itemSet->count; i++) {
        xpc_object_t item = xpc_dictionary_create(NULL, NULL, 0);
        require(item != NULL, done);
        
        xpc_dictionary_set_string(item, AUTH_XPC_ITEM_NAME, itemSet->items[i].name);
        xpc_dictionary_set_uint64(item, AUTH_XPC_ITEM_FLAGS, itemSet->items[i].flags);
        xpc_dictionary_set_data(item, AUTH_XPC_ITEM_VALUE, itemSet->items[i].value, itemSet->items[i].valueLength);
        xpc_array_set_value(set, XPC_ARRAY_APPEND, item);
        xpc_release(item);
    }
    
done:
    return set;
}

AuthorizationItemSet *
DeserializeItemSet(const xpc_object_t data)
{
    AuthorizationItemSet * set = NULL;
    require_quiet(data != NULL, done);
    xpc_retain(data);
    require(xpc_get_type(data) == XPC_TYPE_ARRAY, done);
    
    set = (AuthorizationItemSet*)calloc(1u, sizeof(AuthorizationItemSet));
    require(set != NULL, done);
    
    set->count = (uint32_t)xpc_array_get_count(data);
    if (set->count) {
        set->items = (AuthorizationItem*)calloc(set->count, sizeof(AuthorizationItem));
        require_action(set->items != NULL, done, set->count = 0);
        
        xpc_array_apply(data, ^bool(size_t index, xpc_object_t value) {
            void *dataCopy = 0;
            require(xpc_get_type(value) == XPC_TYPE_DICTIONARY, done);
            size_t nameLen = 0;
            const char * name = xpc_dictionary_get_string(value, AUTH_XPC_ITEM_NAME);
            if (name) {
                nameLen = strlen(name) + 1;
                set->items[index].name = calloc(1u, nameLen);
                require(set->items[index].name != NULL, done);
                
                strlcpy((char*)set->items[index].name, name, nameLen);
            }
            set->items[index].flags = (uint32_t)xpc_dictionary_get_uint64(value, AUTH_XPC_ITEM_FLAGS);
            size_t len;
            const void * valueData = xpc_dictionary_get_data(value, AUTH_XPC_ITEM_VALUE, &len);

            // <rdar://problem/13033889> authd is holding on to multiple copies of my password in the clear
            if (xpc_dictionary_get_value(value, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH) != NULL) {
                size_t sensitiveLength = (size_t)xpc_dictionary_get_uint64(value, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH);
                dataCopy = malloc(sensitiveLength);
                require(dataCopy != NULL, done);
                memcpy(dataCopy, valueData, sensitiveLength);
                memset_s((void *)valueData, len, 0, sensitiveLength); // clear the sensitive data, memset_s is never optimized away
                len = sensitiveLength;
            } else {
                dataCopy = malloc(len);
                require(dataCopy != NULL, done);
                memcpy(dataCopy, valueData, len);
            }

            set->items[index].valueLength = len;
            if (len) {
                set->items[index].value = calloc(1u, len);
                require(set->items[index].value != NULL, done);
                
                memcpy(set->items[index].value, dataCopy, len);
            }

        done:
            if (dataCopy)
                free(dataCopy);
            return true;
        });
    }
    
done:
    if (data != NULL) {
        xpc_release(data);
    }
    return set;
}

void FreeItemSet(AuthorizationItemSet * itemSet)
{
    if (!itemSet) { return; }
    
    for(uint32_t i = 0; i < itemSet->count; i++ ) {
        if (itemSet->items[i].name) {
            free((void*)itemSet->items[i].name);
        }
        if (itemSet->items[i].value) {
            free(itemSet->items[i].value);
        }
    }
    if (itemSet->items) {
        free(itemSet->items);
    }

    free(itemSet);
}

char *
_copy_cf_string(CFTypeRef str, const char * defaultValue)
{
    char * result = NULL;
    require(str != NULL, done);   
    require(CFGetTypeID(str) == CFStringGetTypeID(), done);
    
    CFIndex length = CFStringGetLength(str);
    CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    
    result = (char*)calloc(1u, (size_t)size);
    check(result != NULL);
    
    if (!CFStringGetCString(str, result, size, kCFStringEncodingUTF8)) {
        free_safe(result);
    }
    
done:
    if (result == NULL && defaultValue) {
        size_t len = strlen(defaultValue);
        result = (char*)calloc(1u, len);
        check(result != NULL);
        
        strlcpy(result, defaultValue, len);
    }
    
    return result;
}

int64_t
_get_cf_int(CFTypeRef num, int64_t defaultValue)
{
    int64_t result = defaultValue;
    require(num != NULL, done);
    require(CFGetTypeID(num) == CFNumberGetTypeID(), done);
    
    if (!CFNumberGetValue(num, kCFNumberSInt64Type, &result)) {
        result = defaultValue;
    }
    
done:
    return result;
}

bool
_get_cf_bool(CFTypeRef value, bool defaultValue)
{
    bool result = defaultValue;
    require(value != NULL, done);
    require(CFGetTypeID(value) == CFBooleanGetTypeID(), done);
    
    result = CFBooleanGetValue(value);
    
done:
    return result;
}

bool
_compare_string(const char * str1, const char * str2)
{
    if (!(str1 == str2)) {  // compare null or same pointer
        if (str1 && str2) { // check both are non null
            if (strcasecmp(str1, str2) != 0) { // compare strings
                return false; // return false if not equal
            }
        } else {
            return false; // return false if one null
        }
    }
    
    return true;
}

char *
_copy_string(const char * str)
{
    char * result = NULL;
    require(str != NULL, done);
    
    size_t len = strlen(str) + 1;
    result = calloc(1u, len);
    require(result != NULL, done);
    
    strlcpy(result, str, len);
    
done:
    return result;
}

void *
_copy_data(const void * data, size_t dataLen)
{
    void * result = NULL;
    require(data != NULL, done);
    
    result = calloc(1u, dataLen);
    require(result != NULL, done);
    
    memcpy(result, data, dataLen);
    
done:
    return result;
}

bool _cf_set_iterate(CFSetRef set, bool(^iterator)(CFTypeRef value))
{
    bool result = false;
    CFTypeRef* values = NULL;
    
    require(set != NULL, done);
    
    CFIndex count = CFSetGetCount(set);
    values = calloc((size_t)count, sizeof(CFTypeRef));
    require(values != NULL, done);
    
    CFSetGetValues(set, values);
    for (CFIndex i = 0; i < count; i++) {
        result = iterator(values[i]);
        if (!result) {
            break;
        }
    }

done:
    free_safe(values);
    return result;
}

bool _cf_bag_iterate(CFBagRef bag, bool(^iterator)(CFTypeRef value))
{
    bool result = false;
    CFTypeRef* values = NULL;

    require(bag != NULL, done);

    CFIndex count = CFBagGetCount(bag);
    values = calloc((size_t)count, sizeof(CFTypeRef));
    require(values != NULL, done);

    CFBagGetValues(bag, values);
    for (CFIndex i = 0; i < count; i++) {
        result = iterator(values[i]);
        if (!result) {
            break;
        }
    }

done:
    free_safe(values);
    return result;
}

bool _cf_dictionary_iterate(CFDictionaryRef dict, bool(^iterator)(CFTypeRef key, CFTypeRef value))
{
    bool result = false;
    CFTypeRef* keys = NULL;
    CFTypeRef* values = NULL;
    
    require(dict != NULL, done);
    
    CFIndex count = CFDictionaryGetCount(dict);
    keys = calloc((size_t)count, sizeof(CFTypeRef));
    require(keys != NULL, done);
    
    values = calloc((size_t)count, sizeof(CFTypeRef));
    require(values != NULL, done);
    
    CFDictionaryGetKeysAndValues(dict, keys, values);
    for (CFIndex i = 0; i < count; i++) {
        result = iterator(keys[i], values[i]);
        if (!result) {
            break;
        }
    }

done:
    free_safe(keys);
    free_safe(values);
    return result;
}
