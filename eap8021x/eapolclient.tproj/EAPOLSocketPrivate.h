/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

/* 
 * Modification History
 *
 * May 21, 2008	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_EAPOLSOCKETPRIVATE_H
#define _S_EAPOLSOCKETPRIVATE_H
#include <stdio.h>
#include <net/ethernet.h>
#include <SystemConfiguration/SCPreferences.h>
#include "Supplicant.h"

typedef struct EAPOLSocketSource_s EAPOLSocketSource, *EAPOLSocketSourceRef;

EAPOLSocketSourceRef
EAPOLSocketSourceCreate(const char * if_name,
			const struct ether_addr * ether,
			CFDictionaryRef * control_dict_p);
void
EAPOLSocketSourceFree(EAPOLSocketSourceRef * source_p);

SupplicantRef
EAPOLSocketSourceCreateSupplicant(EAPOLSocketSourceRef source,
				  CFDictionaryRef control_dict);
void
EAPOLSocketSetGlobals(SCPreferencesRef prefs);

#endif /* _S_EAPOLSOCKETPRIVATE_H */

