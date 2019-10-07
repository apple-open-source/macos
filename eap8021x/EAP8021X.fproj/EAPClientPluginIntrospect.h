/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPCLIENTPLUGIN_INTROSPECT_H
#define _EAP8021X_EAPCLIENTPLUGIN_INTROSPECT_H

typedef void * (*EAPClientPluginFuncRef)();

typedef const char * EAPClientPluginFuncName;

EAPClientPluginFuncRef
md5_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
leap_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eaptls_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapttls_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
peap_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapmschapv2_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapgtc_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapfast_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapsim_introspect(EAPClientPluginFuncName name);

EAPClientPluginFuncRef
eapaka_introspect(EAPClientPluginFuncName name);

#endif /* _EAP8021X_EAPCLIENTPLUGIN_INTROSPECT_H */
