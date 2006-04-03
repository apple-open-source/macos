/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <mach/mach.h>

/* The currently known Mac OS X deployment targets */
enum macosx_deployment_target_value {
    MACOSX_DEPLOYMENT_TARGET_10_1,
    MACOSX_DEPLOYMENT_TARGET_10_2,
    MACOSX_DEPLOYMENT_TARGET_10_3,
    MACOSX_DEPLOYMENT_TARGET_10_4,
    MACOSX_DEPLOYMENT_TARGET_10_5
};

__private_extern__ void get_macosx_deployment_target(
    enum macosx_deployment_target_value *value,
    const char **name,
    cpu_type_t cputype);

__private_extern__ void put_macosx_deployment_target(
    char *target);
