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

#ifndef json_support_h
#define json_support_h

#include <CoreFoundation/CoreFoundation.h>

#pragma mark -

/*
* Add object to an existing dictionary functions
*/
int json_dict_add_dict(CFMutableDictionaryRef dict, const char *key,
                   const CFMutableDictionaryRef value);
int json_dict_add_array(CFMutableDictionaryRef dict, const char *key,
                   const CFMutableArrayRef value);
int json_dict_add_num(CFMutableDictionaryRef dict, const char *key,
                  const void *value, size_t size);
int json_dict_add_str(CFMutableDictionaryRef dict, const char *key,
                  const char *value);

/*
* Add object to an existing array functions
*/
int json_arr_add_str(CFMutableArrayRef arr,
                     const char *value);
int json_arr_add_dict(CFMutableArrayRef arr,
                     const CFMutableDictionaryRef value);

#pragma mark -

/*
* Print out a Core Foundation object in JSON format
*/

int json_print_cf_object(CFTypeRef cf_object, char *output_file_path);

#endif /* json_support_h */
