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
#include "ipsecConfigTracer.h"
#include "ipsecMessageTracer.h"

const char * ipsecConfigTracerFailedString = "Tracer Failed";
const char * ipsecConfigInvalidEventString = "Invalid Event";
const char * ipsecConfigString			   = "IPSEC";

const char * const ipsecConfigEventStrings[IPSECCONFIGEVENTCODE_MAX] =	{	CONSTSTR("NONE") /* index place holder */,
																			CONSTSTR("Configuration Reparse Error"),
																			CONSTSTR("Configuration Parse Error"),
                                                                            CONSTSTR("Signal Error"),
																		};

const char *
ipsecConfigEventCodeToString (ipsecConfigEventCode_t eventCode)
{
	if (eventCode <= IPSECCONFIGEVENTCODE_NONE || eventCode >= IPSECCONFIGEVENTCODE_MAX)
		return ipsecConfigInvalidEventString;
	return(ipsecConfigEventStrings[eventCode]);
}

static
void
ipsecConfigLogEvent (const char *event_msg, const char *failure_signature)
{
	aslmsg m;
    
	if (!event_msg) {
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, ASL_KEY_FACILITY, PLAINIPSECDOMAIN);
	asl_set(m, ASL_KEY_MSG, ipsecConfigString);
#if 0   /* <rdar://problem/6468252> is flooding 300000+ events to MessageTracer servers */ 
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
ipsecConfigTracerEvent (const char *filename, ipsecConfigEventCode_t eventCode, const char *event, const char *failure_reason)
{
	char buf[1024];

	if (filename == NULL) {
		ipsecConfigLogEvent(CONSTSTR("tracer failed. (Invalid filename)."), ipsecConfigTracerFailedString);
		return;
	}
	if (eventCode <= IPSECCONFIGEVENTCODE_NONE || eventCode >= IPSECCONFIGEVENTCODE_MAX) {
		ipsecConfigLogEvent(CONSTSTR("tracer failed. (Invalid event code)."), ipsecConfigTracerFailedString);
		return;
	}
	if (event == NULL) {
		ipsecConfigLogEvent(CONSTSTR("tracer failed. (Invalid event)."), ipsecConfigTracerFailedString);
		return;
	}
	
	buf[0] = (char)0;
	snprintf(buf, sizeof(buf), "%s. (%s, filename %s).", ipsecConfigEventCodeToString(eventCode), failure_reason, filename);
	ipsecConfigLogEvent(CONSTSTR(buf), event);
}
