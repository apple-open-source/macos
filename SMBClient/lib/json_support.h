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

/*
 Example output

 ./getattrlist -b -f JSON /Volumes/Data/SMBBasic/AppleTest CMN_NAME,ATTR_CMN_OBJTYPE
 {
   "inputs" : {
     "api" : "getattrlistbulk",
     "attributes" : "ATTR_CMN_RETURNED_ATTRS,ATTR_CMN_NAME,ATTR_CMN_OBJTYPE",
     "options" : "<none>",
     "path" : "\/Volumes\/Data\/SMBBasic\/AppleTest",
     "recurse" : "no"
   },
   "outputs" : {
     "\/Volumes\/Data\/SMBBasic\/AppleTest" : {
       "dir_entry_count" : 3,
       "entry-0" : {
         "ATTR_CMN_NAME" : ".DS_Store",
         "ATTR_CMN_OBJTYPE" : "VREG",
         "ATTR_CMN_RETURNED_ATTRS" : "Requested - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000                        Returned  - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000",
         "attr_len" : 48
       },
       "entry-1" : {
         "ATTR_CMN_NAME" : "ShareFolder1",
         "ATTR_CMN_OBJTYPE" : "VDIR",
         "ATTR_CMN_RETURNED_ATTRS" : "Requested - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000                        Returned  - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000",
         "attr_len" : 56
       },
       "entry-2" : {
         "ATTR_CMN_NAME" : "ShareFolder2",
         "ATTR_CMN_OBJTYPE" : "VDIR",
         "ATTR_CMN_RETURNED_ATTRS" : "Requested - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000                        Returned  - commonattr 0x80000009 volattr 0x00000000 dirattr 0x00000000 fileattr 0x00000000 forkattr 0x00000000",
         "attr_len" : 56
       },
       "timings" : {
         "duration_usec" : 615
       }
     }
   },
   "results" : {
     "error" : 0
   }
 }

 */


#pragma mark -

/*
* Add object to an existing dictionary functions
*/

int json_add_cfstr(CFMutableDictionaryRef dict, const char *key,
                    const CFMutableStringRef value);
int json_add_dict(CFMutableDictionaryRef dict, const char *key,
                   const CFMutableDictionaryRef value);
int json_add_num(CFMutableDictionaryRef dict, const char *key,
                  const void *value, size_t size);
int json_add_str(CFMutableDictionaryRef dict, const char *key,
                  const char *value);


#pragma mark -

/*
* Special purpose dictionaries inside of a parent dictionary
* If the special dictionary does not already exist inside the parent
* dictionary, then create it and add it into the parent dictionary.
*/
int json_add_inputs_str(CFMutableDictionaryRef dict, const char *key,
                        const void *value);
int json_add_outputs_dict(CFMutableDictionaryRef dict, const char *key,
                          const CFMutableDictionaryRef value);
int json_add_outputs_str(CFMutableDictionaryRef dict, const char *key,
                         const void *value);
int json_add_results(CFMutableDictionaryRef dict, const char *key,
                     const void *value, size_t size);
int json_add_results_str(CFMutableDictionaryRef dict, const char *key,
                         const void *value);
int json_add_time_stamp(CFMutableDictionaryRef dict, const char *key);
int json_add_timing(CFMutableDictionaryRef dict, const char *key,
                     const void *value, size_t size);


#pragma mark -

/*
* Print out a Core Foundation object in JSON format
*/

int json_print_cf_object(CFTypeRef cf_object, char *output_file_path);

#endif /* json_support_h */
