/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOLCLIENT_H
#define _EAP8021X_EAPOLCLIENT_H
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>

typedef struct EAPOLClient_s EAPOLClient, * EAPOLClientRef;
typedef void (EAPOLClientCallBack) (EAPOLClientRef client, Boolean server_died,
				    void * context);
typedef EAPOLClientCallBack * EAPOLClientCallBackRef;

EAPOLClientRef
EAPOLClientAttach(const char * interface_name, EAPOLClientCallBack callback,
		  void * context, CFDictionaryRef * config, 
		  int * result);

int
EAPOLClientDetach(EAPOLClientRef * client);

int
EAPOLClientGetConfig(EAPOLClientRef client, CFDictionaryRef * config_dict);

int
EAPOLClientReportStatus(EAPOLClientRef client, CFDictionaryRef status_dict);

int
EAPOLClientForceRenew(EAPOLClientRef client);

#if ! TARGET_OS_EMBEDDED
int
EAPOLClientUserCancelled(EAPOLClientRef client);
#endif /* ! TARGET_OS_EMBEDDED */

#endif /* _EAP8021X_EAPOLCONTROL_H */
