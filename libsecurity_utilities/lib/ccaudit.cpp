/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#include <strings.h>	// bcopy()
#include <errno.h>
#include <bsm/libbsm.h>
#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>
#include <security_utilities/errors.h>
#include <security_utilities/ccaudit.h>

namespace Security
{
namespace CommonCriteria
{

TerminalId::TerminalId()
{
	if (audit_set_terminal_id(this) != kAUNoErr)
	{
		Syslog::warning("setting terminal ID info failed; using defaults");
		port = 0;
		machine = 0;
	}
}

AuditToken::AuditToken(const audit_token_t &token)
{
    audit_token_to_au32(token, &mAuditId, &mEuid, &mEgid, &mRuid, &mRgid, &mPid, &mAuditSessionId, &mTerminalId);
}

void AuditSession::registerSession(void)
{
    auditinfo_t auinfo;

    auinfo.ai_auid = mAuditId;
    auinfo.ai_asid = mSessionId;
    bcopy(&mTerminalId, &(auinfo.ai_termid), sizeof(auinfo.ai_termid));
    bcopy(&mEventMask.get(), &(auinfo.ai_mask), sizeof(auinfo.ai_mask));

    if (setaudit(&auinfo) != 0)
	{
		if (errno == ENOTSUP)
			Syslog::notice("Attempted to initialize auditing, but this kernel does not support auditing");
		else
			Syslog::warning("Could not initialize auditing (%m); continuing");
	}
}

void AuditRecord::submit(const short event_code, const int returnCode, 
			 const char *msg)
{
    // If we're not auditing, do nothing
    if (!(au_get_state() == AUC_AUDITING))
		return;

    secdebug("ccaudit", "Submitting authorization audit record");

    int ret = kAUNoErr;

    // XXX/gh  3574731: Fix BSM SPI so the const_cast<>s aren't necessary
    if (returnCode == 0)
    {
		token_t *tok = NULL;

		if (msg)
			tok = au_to_text(const_cast<char *>(msg));
		ret = audit_write_success(event_code, const_cast<token_t *>(tok), 
								  mAuditToken.auditId(), mAuditToken.euid(),
								  mAuditToken.egid(), mAuditToken.ruid(), 
								  mAuditToken.rgid(), mAuditToken.pid(), 
								  mAuditToken.auditSession(),
								  const_cast<au_tid_t *>(&(mAuditToken.terminalId())));
    }
    else
    {
		ret = audit_write_failure(event_code, const_cast<char *>(msg), 
								  returnCode, mAuditToken.auditId(), 
								  mAuditToken.euid(), mAuditToken.egid(), 
								  mAuditToken.ruid(), mAuditToken.rgid(), 
								  mAuditToken.pid(), mAuditToken.auditSession(),
								  const_cast<au_tid_t *>(&(mAuditToken.terminalId())));
    }
    if (ret != kAUNoErr)
		MacOSError::throwMe(ret);
}


}	// end namespace CommonCriteria
}	// end namespace Security
