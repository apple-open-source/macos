/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#ifndef _EAPOLCFG_AUTH_TYPES_H
#define _EAPOLCFG_AUTH_TYPES_H

/*
 * Keep IPC functions private to the framework
 */
#ifdef mig_external
#undef mig_external
#endif
#define mig_external __private_extern__

#if 0
/* Turn MIG type checking on by default */
#ifdef __MigTypeCheck
#undef __MigTypeCheck
#endif
#define __MigTypeCheck	1
#endif /* 0 */

/*
 * Mach server port name
 */
#define EAPOLCFG_AUTH_SERVER	"com.apple.eapolcfg_auth"

enum {
    keapolcfg_auth_set_name		= 0x1,
    keapolcfg_auth_set_password		= 0x2
};
typedef const char * xmlData_t;
typedef const char * OOBData_t;
typedef char * OOBDataOut_t;

#endif /* _EAPOLCFG_AUTH_TYPES_H */
