/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOLUTIL_H
#define _EAP8021X_EAPOLUTIL_H


/* 
 * Modification History
 *
 * September 3, 2010	Dieter Siegmund (dieter@apple.com)
 * - moved here from EAPOLSocket.c
 */

/*
 * EAPOLUtil.h
 * - EAPOL utility functions
 */

#include <EAP8021X/EAPOL.h>
#include <stdio.h>

bool
EAPOLPacketValid(EAPOLPacketRef eapol_p, unsigned int length, FILE * f);

#endif /* _EAP8021X_EAPOLUTIL_H */

