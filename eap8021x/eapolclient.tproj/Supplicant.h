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

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_SUPPLICANT_H
#define _S_SUPPLICANT_H

#include <CoreFoundation/CFDictionary.h>
#include <sys/types.h>
#include <net/if_dl.h>
#include "EAPClientTypes.h"
#include "SupplicantTypes.h"
#include "EAPOLControlTypes.h"
#include "EAPOLSocket.h"

typedef struct Supplicant_s Supplicant, *SupplicantRef;

SupplicantRef 
Supplicant_create(EAPOLSocketRef sock);

SupplicantRef
Supplicant_create_with_supplicant(EAPOLSocketRef sock, SupplicantRef main_supp);

void
Supplicant_free(SupplicantRef * supp_p);

void
Supplicant_start(SupplicantRef supp);

void
Supplicant_set_no_ui(SupplicantRef supp);

void
Supplicant_set_debug(SupplicantRef supp, bool debug);

void
Supplicant_link_status_changed(SupplicantRef supp, bool active);

SupplicantState
Supplicant_get_state(SupplicantRef supp, EAPClientStatus * last_status);

bool
Supplicant_control(SupplicantRef supp,
		   EAPOLClientControlCommand command,
		   CFDictionaryRef control_dict);

bool
Supplicant_update_configuration(SupplicantRef supp,
				CFDictionaryRef config_dict,
				bool * should_stop);

void
Supplicant_stop(SupplicantRef supp);

void
Supplicant_simulate_success(SupplicantRef supp);

#endif /* _S_SUPPLICANT_H */

