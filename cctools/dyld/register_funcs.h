/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <mach-o/loader.h>
#import "images.h"

extern void register_func_for_add_image(
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide));
extern void call_registered_funcs_for_add_images(
    void);

extern void register_func_for_remove_image(
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide));
extern void call_funcs_for_remove_image(
    struct mach_header *mh,
    unsigned long vmaddr_slide);

extern void register_func_for_link_module(
    void (*func)(module_state *module));
extern void call_registered_funcs_for_linked_modules(
    void);

extern void register_func_for_unlink_module(
    void (*func)(module_state *module));

extern void register_func_for_replace_module(
    void (*func)(module_state *oldmodule, module_state *newmodule));
