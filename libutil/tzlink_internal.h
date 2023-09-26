/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#define TZLINK_SERVICE_NAME "com.apple.tzlink"

#define TZLINK_ENTITLEMENT "com.apple.tzlink.allow"

#define TZLINK_KEY_REQUEST_TIMEZONE "tz" // string
#define TZLINK_KEY_REPLY_ERROR "error" // uint64

// These also probably don't need to be here
#define KERN_APFSPREBOOTUUID "kern.apfsprebootuuid"
#define KERN_BOOTUUID "kern.bootuuid"

// These could probably also be cleaned up since they're not used
int get_preboot_volume_uuid(char **uuid_out);
const char *construct_preboot_volume_path(const char *format_string, const char *uuid);
int check_update_timezone_db(char *preboot_volume_uuid);
int update_preboot_volume(const char *localtime_target_path);
int get_tz_version(const char *tz_path, char **tz_version);
char *file_path_append(const char *trunk, const char *suffix);
int remove_files(const char* path);
