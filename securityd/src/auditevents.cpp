/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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


//
// auditevents - monitor and act upon audit subsystem events
//
#include "auditevents.h"
#include "dtrace.h"
#include <security_utilities/logging.h>
#include "self.h"

using namespace UnixPlusPlus;
using namespace MachPlusPlus;


AuditMonitor::AuditMonitor(Port relay)
	: mRelay(relay)
{
}

AuditMonitor::~AuditMonitor()
{
}


//
// Endlessly retrieve session events and dispatch them.
// (The current version of MachServer cannot receive FileDesc-based events,
// so we need a monitor thread for this.)
//
void AuditMonitor::action()
{
	au_sdev_handle_t *dev = au_sdev_open(AU_SDEVF_ALLSESSIONS);
	int event;
	auditinfo_addr_t aia;

	if (NULL == dev) {
		Syslog::error("This is bad, man. I've got bad vibes here. Could not open %s: %d", AUDIT_SDEV_PATH, errno);
		return;
	}

	for (;;) {
		if (0 != au_sdev_read_aia(dev, &event, &aia)) {
			Syslog::error("au_sdev_read_aia failed: %d\n", errno);
			continue;
		}
		SECURITYD_SESSION_NOTIFY(aia.ai_asid, event, aia.ai_auid);
		if (kern_return_t rc = self_client_handleSession(mRelay, mach_task_self(), event, aia.ai_asid))
			Syslog::error("self-send failed (mach error %d)", rc);
	}
}
