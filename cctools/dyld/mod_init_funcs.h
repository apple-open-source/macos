/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
extern void call_module_initializers(
    enum bool make_delayed_calls,
    enum bool bind_now,
    enum bool post_launch_libraries_only);

extern void call_module_initializers_for_library(
    struct library_image *library_image,
#ifdef __ppc__
    double *fp_save_area,
    unsigned long *vec_save_area,
    enum bool *saved_regs,
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
    int *facilities_used,
#endif /* !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) */
#endif /* __ppc__ */
    enum bool make_delayed_calls,
    enum bool bind_now,
    enum bool post_launch_libraries_only);

extern void call_module_terminator_for_object(
    struct object_image *object_image);

extern void _dyld_call_module_initializers_for_dylib(
    struct mach_header *mh_dylib_header);

extern void _dyld_mod_term_funcs(
    void);
