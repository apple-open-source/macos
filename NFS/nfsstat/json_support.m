/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#include "json_support.h"

#pragma mark -

/*
* Helper functions
*/

static CFMutableStringRef
create_cfstr(const char *string)
{
    CFMutableStringRef cf_str = CFStringCreateMutable(NULL, 0);
    if (cf_str == NULL) {
        fprintf(stderr, "*** %s: CFStringCreateMutable failed \n", __FUNCTION__);
        return NULL;
    }
    CFStringAppendCString(cf_str, string, kCFStringEncodingUTF8);

    /* Replace any "\n" in the strings with " " */
    CFStringFindAndReplace(cf_str, CFSTR("\n"), CFSTR(" "),
                           CFRangeMake(0, CFStringGetLength(cf_str)), 0);

    return(cf_str);
}


#pragma mark -

/*
 * Add object to an existing dictionary functions
 */

int
json_dict_add_dict(CFMutableDictionaryRef dict, const char *key,
              const CFMutableDictionaryRef value)
{
    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(ENOMEM);
    }

    CFDictionarySetValue(dict, cf_key, value);

    CFRelease(cf_key);
    return(0);
}

int
json_dict_add_array(CFMutableDictionaryRef dict, const char *key,
              const CFMutableArrayRef value)
{
    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(ENOMEM);
    }

    CFDictionarySetValue(dict, cf_key, value);

    CFRelease(cf_key);
    return(0);
}

int
json_dict_add_num(CFMutableDictionaryRef dict, const char *key,
             const void *value, size_t size)
{
    CFNumberRef cf_num = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(ENOMEM);
    }

    switch(size) {
        case sizeof(uint8_t):
            cf_num = CFNumberCreate(NULL, kCFNumberSInt8Type, value);
            break;
        case sizeof(uint16_t):
            cf_num = CFNumberCreate(NULL, kCFNumberSInt16Type, value);
            break;
        case sizeof(uint32_t):
            cf_num = CFNumberCreate(NULL, kCFNumberSInt32Type, value);
            break;
        case sizeof(uint64_t):
            cf_num = CFNumberCreate(NULL, kCFNumberSInt64Type, value);
            break;
        default:
            fprintf(stderr, "*** %s: Unsupported size %zu \n", __FUNCTION__, size);
            CFRelease(cf_key);
            return(EINVAL);
    }

    if (cf_num == NULL) {
        fprintf(stderr, "*** %s: CFNumberCreate failed \n", __FUNCTION__);
        CFRelease(cf_key);
        return(ENOMEM);
    }

    CFDictionarySetValue(dict, cf_key, cf_num);

    CFRelease(cf_key);
    CFRelease(cf_num);
    return(0);
}

int
json_dict_add_str(CFMutableDictionaryRef dict, const char *key,
             const char *value)
{
    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(ENOMEM);
    }

    CFMutableStringRef cf_val = create_cfstr(value);
    if (cf_val == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, value);
        CFRelease(cf_key);
        return(ENOMEM);
    }

    CFDictionarySetValue(dict, cf_key, cf_val);

    CFRelease(cf_key);
    CFRelease(cf_val);
    return(0);
}

int
json_arr_add_str(CFMutableArrayRef arr, const char *value)
{
    if ((arr == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: arr or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFMutableStringRef cf_val = create_cfstr(value);
    if (cf_val == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, value);
        return(ENOMEM);
    }

    CFArrayAppendValue(arr, cf_val);

    CFRelease(cf_val);
    return(0);
}

int
json_arr_add_dict(CFMutableArrayRef arr, const CFMutableDictionaryRef value)
{
    if ((arr == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: arr or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    CFArrayAppendValue(arr, value);

    return(0);
}

#pragma mark -

/*
 * Print out a Core Foundation object in JSON format
 */

int
json_print_cf_object(CFTypeRef cf_object, char *output_file_path)
{
    @autoreleasepool {
        NSError * error = nil;
        NSObject *ns_object = CFBridgingRelease(cf_object);
        NSOutputStream *outputStream = NULL;
        NSString *pathStr = NULL;
        NSData *data = NULL;

        if (![NSJSONSerialization isValidJSONObject:ns_object]) {
            fprintf(stderr, "*** %s: Invalid JSON object \n", __FUNCTION__);
            NSLog(@"%@", ns_object);
            return(EINVAL);
        }

        if (output_file_path == NULL) {
            /*
             * Write JSON output to stdout
             *
             * It would be so much easier to just use
             * NSOutputStream *outputStream = [NSOutputStream outputStreamToFileAtPath:@"/dev/stdout" append:NO];
             * Unfortunately, I randomly get an error from writeJSONOBject about
             * "The file couldn't be saved because there isn't enough space".
             *
             * To work around this, write to NSData, then write it out
             */
            data = [NSJSONSerialization dataWithJSONObject:ns_object
                                                   options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                                     error:&error];
            if (error) {
                /* NSLog goes to stderr always */
                NSLog(@"*** %s: dataWithJSONObject failed %@",
                      __FUNCTION__, error);
                return(EINVAL);
            }

            [[NSFileHandle fileHandleWithStandardOutput] writeData:data
                                                             error:&error];
            if (error) {
                /* NSLog goes to stderr always */
                NSLog(@"*** %s: fileHandleWithStandardOutput failed %@",
                      __FUNCTION__, error);
                return(EINVAL);
            }
        }
        else {
            /* Write JSON output to a file */
            pathStr = [[NSString alloc]initWithCString:output_file_path
                                              encoding:NSUTF8StringEncoding];

            outputStream = [NSOutputStream outputStreamToFileAtPath:pathStr
                                                             append:NO];

            [outputStream open];

            [NSJSONSerialization writeJSONObject:ns_object
                                        toStream:outputStream
                                         options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                           error:&error];
            if (error) {
                /* NSLog goes to stderr always */
                NSLog(@"*** %s: writeJSONObject failed %@", __FUNCTION__, error);
                return(EINVAL);
            }

           [outputStream close];
       }

    }
    return(0);
}
