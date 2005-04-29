/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

typedef boolean_t samp_skip_t (void);
typedef boolean_t samp_print_t (const char *a_format, ...);
typedef boolean_t samp_vprint_t (boolean_t a_newline, const char *a_format,
    va_list a_p);

boolean_t
samp_init(samp_skip_t *a_skipl, samp_print_t *a_printl, samp_print_t *a_println,
    samp_vprint_t *a_vprintln, samp_vprint_t *a_veprint);
void
samp_fini(void);
boolean_t
samp_run(void);
