/* srp-clientstub.c
 *
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file includes the dnssd client stub implementation locally so that we can add some functionality
 * without exporting data structures that are private to the client stub.
 */

#define SRP_CLIENT_STUB
#include "../mDNSShared/dnssd_clientstub.c"
#include "srp-clientstub.h"

bool
DNSServiceRecordValidate(DNSServiceRef sdref, DNSRecordRef rref)
{
    if (!sdref) {
        syslog(LOG_WARNING, "DNSServiceRecordValidate called with NULL DNSServiceRef");
        return false;
    }
    if (!rref) {
        syslog(LOG_WARNING, "DNSServiceRecordValidate called with NULL DNSRecordRef");
        return false;
    }
    if (!sdref->max_index) {
        syslog(LOG_WARNING, "DNSServiceRecordValidate called with bad DNSServiceRef");
        return false;
    }

    if (!DNSServiceRefValid(sdref)) {
        syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRecordValidate called with invalid DNSServiceRef %p %08X %08X",
            sdref, (unsigned int)sdref->sockfd, (unsigned int)sdref->validator);
        return false;
    }

    // Ensure that this rref is actually dependent on the sdref. An rref can't not be dependent on an sdref.
    DNSRecord **p = &sdref->rec;
    while (*p && *p != rref) p = &(*p)->recnext;
    if (*p == NULL) {
        syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRecordValidate called with invalid DNSRecordRef %p %08X %08X",
            rref, (unsigned int)sdref->sockfd, (unsigned int)sdref->validator);
        return false;
    }
    return true;
}

void
DNSServiceRecordSetCallback(DNSServiceRef sdref, DNSRecordRef rref, DNSServiceRegisterRecordReply callBack, void *context)
{
    if (DNSServiceRecordValidate(sdref, rref)) {
		rref->AppContext = context;
		rref->AppCallback = callBack;
	}
}

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
