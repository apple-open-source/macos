/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#import	 <asl.h>
#include <sys/types.h>
#include "ipsecPolicyTracer.h"
#include "ipsecMessageTracer.h"

const char *ipsecConfigTracerFailedString = "Tracer Failed";
const char *ipsecPolicyInvalidEventString = "Invalid Event";
const char *ipsecPolicyString			  = "IPSEC";

const char * const ipsecPolicyEventStrings[IPSECPOLICYEVENTCODE_MAX] =	{	CONSTSTR("NONE") /* index place holder */,
																			CONSTSTR("setkey Error"),
																		};

const char *
ipsecPolicyEventCodeToString (ipsecPolicyEventCode_t eventCode)
{
	if (eventCode <= IPSECPOLICYEVENTCODE_NONE || eventCode >= IPSECPOLICYEVENTCODE_MAX)
		return ipsecPolicyInvalidEventString;
	return(ipsecPolicyEventStrings[eventCode]);
}

static
void
ipsecPolicyLogEvent (const char *event_msg, const char *failure_signature)
{
	aslmsg m;

	if (!event_msg) {
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, ASL_KEY_FACILITY, PLAINIPSECDOMAIN);
	asl_set(m, ASL_KEY_MSG, ipsecPolicyString);
#if 0 /* we don't want to send filenames to MessageTracer server */
    if (failure_signature) {
        asl_set(m, "com.apple.message.domain", PLAINIPSECDOMAIN);
        asl_set(m, "com.apple.message.result", "failure");	// failure
        asl_set(m, "com.apple.message.signature", failure_signature);
    }
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "%s", event_msg);
#else
    if (failure_signature) {
        asl_log(NULL, m, ASL_LEVEL_NOTICE, "%s (failure: %s)", event_msg, failure_signature);
    } else {
        asl_log(NULL, m, ASL_LEVEL_NOTICE, "%s", event_msg);
    }
#endif
	asl_free(m);
}

void
ipsecPolicyTracerEvent (const char *filename, ipsecPolicyEventCode_t eventCode, const char *event, const char *failure_reason)
{
	char buf[1024];

	if (filename == NULL) {
		ipsecPolicyLogEvent(CONSTSTR("tracer failed. (Invalid filename)."), ipsecConfigTracerFailedString);
		return;
	}
	if (eventCode <= IPSECPOLICYEVENTCODE_NONE || eventCode >= IPSECPOLICYEVENTCODE_MAX) {
		ipsecPolicyLogEvent(CONSTSTR("tracer failed. (Invalid event code)."), ipsecConfigTracerFailedString);
		return;
	}
	if (event == NULL) {
		ipsecPolicyLogEvent(CONSTSTR("tracer failed. (Invalid event)."), ipsecConfigTracerFailedString);
		return;
	}

	buf[0] = (char)0;
	snprintf(buf, sizeof(buf), "%s. (%s, filename %s).", ipsecPolicyEventCodeToString(eventCode), failure_reason, filename);
	ipsecPolicyLogEvent(CONSTSTR(buf), event);	
}
