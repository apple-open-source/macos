/*
 * Copyright (c) 2012-2016 Apple Inc. All rights reserved.
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
 * EAPLog.c
 * - functions to log EAP-related information
 */
/* 
 * Modification History
 *
 * December 26, 2012	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <dispatch/dispatch.h>
#include <SystemConfiguration/SCPrivate.h>
#include "EAPClientPlugin.h"
#include "symbol_scope.h"
#include "EAPLog.h"

STATIC os_log_t S_eap_logger = NULL;

#define EAPOL_OS_LOG_SUBSYSTEM	"com.apple.eapol"

char * const S_eap_os_log_categories[] = {
	"Controller",
	"Monitor",
	"Client",
	"Framework"
};

os_log_t
EAPLogGetLogHandle()
{
    if (S_eap_logger == NULL) {
	EAPLogInit(kEAPLogCategoryFramework);
    }
    return S_eap_logger;
}

#define CHECK_LOG_LEVEL_LIMIT(log_category)				\
	do {												\
	if (log_category < kEAPLogCategoryController ||		\
		log_category > kEAPLogCategoryFramework) {		\
		return;											\
	}													\
	} while(0)

void
EAPLogInit(EAPLogCategory log_category)
{
	CHECK_LOG_LEVEL_LIMIT(log_category);
	S_eap_logger = os_log_create(EAPOL_OS_LOG_SUBSYSTEM,
								 S_eap_os_log_categories[log_category]);
}
