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

/*
 Sample output formatting

 {
   "inputs" : {
     "api" : "getattrlist",
     "attributes" : "ATTR_CMN_NAME",
     "options" : "<none>",
     "path" : "\/Volumes\/Data\/SMBBasic\/AppleTest",
     "recurse" : "no"
   },
   "outputs" : {
     "\/Volumes\/Data\/SMBBasic\/AppleTest" : {
       "entry-0" : {
         "ATTR_CMN_NAME" : "AppleTest",
         "attr_len" : 24
       },
       "timings" : {
         "duration_usec" : 13
       }
     }
   },
   "results" : {
     "error" : 0
   }
 }
 */

#define kInputsKey "inputs"
#define kOutputsKey "outputs"
#define kResultsKey "results"
#define kTimingsKey "timings"

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
json_add_cfstr(CFMutableDictionaryRef dict, const char *key,
               const CFMutableStringRef value)
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

    /* Replace any "\n" in the strings with " " */
    CFStringFindAndReplace(value, CFSTR("\n"), CFSTR(" "),
                           CFRangeMake(0, CFStringGetLength(value)), 0);

    CFDictionarySetValue(dict, cf_key, value);

    CFRelease(cf_key);
    return(0);
}

int
json_add_dict(CFMutableDictionaryRef dict, const char *key,
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
json_add_num(CFMutableDictionaryRef dict, const char *key,
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
json_add_str(CFMutableDictionaryRef dict, const char *key,
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


#pragma mark -

/*
 * Special purpose dictionaries inside of a parent dictionary
 * If the special dictionary does not already exist inside the parent
 * dictionary, then create it and add it into the parent dictionary.
 */

static CFMutableDictionaryRef
get_key_dict(CFMutableDictionaryRef dict, const char *key)
{
    CFMutableDictionaryRef keyDict = NULL;

    if (dict == NULL) {
        fprintf(stderr, "*** %s: dict is null \n", __FUNCTION__);
        return(NULL);
    }

    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(NULL);
    }

    /* Check to see if key dictionary already exists */
    keyDict = (CFMutableDictionaryRef) CFDictionaryGetValue(dict, cf_key);
    if (keyDict == NULL) {
        /* Must not exist yet, so create it and add it */
        keyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
        if (keyDict == NULL) {
            fprintf(stderr, "*** %s: CFDictionaryCreateMutable failed for timings dict \n",
                    __FUNCTION__);
            CFRelease(cf_key);
            return(NULL);
        }
        CFDictionarySetValue(dict, cf_key, keyDict);
        CFRelease(keyDict);
    }

    CFRelease(cf_key);

    return(keyDict);
}

int
json_add_inputs_str(CFMutableDictionaryRef dict, const char *key,
                    const void *value)
{
    CFMutableDictionaryRef resultsDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get inputs dict */
    resultsDict = get_key_dict(dict, kInputsKey);
    if (resultsDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the value to the inputs dictionary */
    json_add_str(resultsDict, key, value);
    return(0);
}

int
json_add_outputs_dict(CFMutableDictionaryRef dict, const char *key,
                      const CFMutableDictionaryRef value)
{
    CFMutableDictionaryRef resultsDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get outputs dict */
    resultsDict = get_key_dict(dict, kOutputsKey);
    if (resultsDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the value to the outputs dictionary */
    json_add_dict(resultsDict, key, value);
    return(0);
}

int
json_add_outputs_str(CFMutableDictionaryRef dict, const char *key,
                     const void *value)
{
    CFMutableDictionaryRef resultsDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get outputs dict */
    resultsDict = get_key_dict(dict, kOutputsKey);
    if (resultsDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the value to the outputs dictionary */
    json_add_str(resultsDict, key, value);
    return(0);
}

int
json_add_results(CFMutableDictionaryRef dict, const char *key,
                 const void *value, size_t size)
{
    CFMutableDictionaryRef resultsDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get results dict */
    resultsDict = get_key_dict(dict, kResultsKey);
    if (resultsDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the value to the results dictionary */
    json_add_num(resultsDict, key, value, size);
    return(0);
}

int
json_add_results_str(CFMutableDictionaryRef dict, const char *key,
                     const void *value)
{
    CFMutableDictionaryRef resultsDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get results dict */
    resultsDict = get_key_dict(dict, kResultsKey);
    if (resultsDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the value to the results dictionary */
    json_add_str(resultsDict, key, value);
    return(0);
}

int
json_add_time_stamp(CFMutableDictionaryRef dict, const char *key)
{
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.dateFormat = @"yyy-MM-dd'T'HH:mm:ss.SSSZZZZZ";
    NSDate *currentTime;
    CFMutableDictionaryRef timingDict = NULL;

    if ((dict == NULL) || (key == NULL)) {
        fprintf(stderr, "*** %s: dict, key is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get timings dict */
    timingDict = get_key_dict(dict, kTimingsKey);
    if (timingDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the time stamp to the timings dictionary */
    CFMutableStringRef cf_key = create_cfstr(key);
    if (cf_key == NULL) {
        fprintf(stderr, "*** %s: create_cfstr failed for \"%s\" \n",
                __FUNCTION__, key);
        return(ENOMEM);
    }

    currentTime = [NSDate date];
    NSString *dateString = [dateFormatter stringFromDate:currentTime];
    CFStringRef cfString = (__bridge CFStringRef)dateString;

    CFDictionarySetValue(timingDict, cf_key, cfString);

    CFRelease(cf_key);
    return(0);
}

int
json_add_timing(CFMutableDictionaryRef dict, const char *key,
                const void *value, size_t size)
{
    CFMutableDictionaryRef timingDict = NULL;

    if ((dict == NULL) || (key == NULL) || (value == NULL)) {
        fprintf(stderr, "*** %s: dict, key or value is null \n", __FUNCTION__);
        return(EINVAL);
    }

    /* Get timings dict */
    timingDict = get_key_dict(dict, kTimingsKey);
    if (timingDict == NULL) {
        fprintf(stderr, "*** %s: get_key_dict failed \n", __FUNCTION__);
        return(ENOMEM);
    }

    /* Add the timing to the timings dictionary */
    json_add_num(timingDict, key, value, size);
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
