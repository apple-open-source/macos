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
#include <unistd.h>	// gethostname()
#include <netdb.h>	// gethostbyname()
#include <sys/types.h>	// inet_addr()
#include <sys/socket.h>	// inet_addr()
#include <netinet/in.h>	// inet_addr()
#include <arpa/inet.h>	// inet_addr()
#include <errno.h>
#include "utilities.h"
#include <Security/logging.h>
#include <bsm/libbsm.h>
#include "ccaudit.h"

namespace Security
{
namespace CommonCriteria
{

void TerminalId::set(void)
{
	if (audit_set_terminal_id(&mTid) != kAUNoErr)
	{
		// If we start seeing the syslog too often, change to secdebug()
		Syslog::warning("setting terminal ID info failed; using defaults");
		mTid.port = 0;
		mTid.machine = 0;
	}
}

void AuditSession::registerSession(void)
{
    auditinfo_t auinfo;

    auinfo.ai_auid = mAuditId;
    auinfo.ai_asid = mSessionId;
    bcopy(&mTerminalId.get(), &(auinfo.ai_termid), sizeof(auinfo.ai_termid));
    bcopy(&mEventMask.get(), &(auinfo.ai_mask), sizeof(auinfo.ai_mask));

    if (setaudit(&auinfo) != 0)
	{
		if (errno == ENOTSUP)
		{
			Syslog::notice("Attempted to initialize auditing, but this kernel that does not support auditing");
			return;
		}
		Syslog::notice("Could not initialize auditing; continuing");
	}
}

void AuditRecord::submit(const short event_code, const int returnCode, 
			 const char *msg)
{
    // If we're not auditing, do nothing
    if (au_get_state() == AUC_NOAUDIT)
		return;

    // XXX  make this a secdebug, then enable it
    // Syslog::notice("Submitting authorization audit record");

    int ret = kAUNoErr;

    // XXX/gh  3574731: Fix BSM SPI so the const_cast<>s aren't necessary
    if (returnCode == 0)
    {
		token_t *tok = NULL;

		if (msg)
			tok = au_to_text(const_cast<char *>(msg));
		ret = audit_write_success(event_code, const_cast<token_t *>(tok), 
								  mAuditId, mEUid, mEGid, mRUid, mRGid, 
								  mPid, mSessionId,
								  const_cast<au_tid_t *>(&(mTerminalId.get())));
    }
    else
    {
		ret = audit_write_failure(event_code, const_cast<char *>(msg), 
								  returnCode, mAuditId, mEUid, mEGid, 
								  mRUid, mRGid, mPid, mSessionId,
								  const_cast<au_tid_t *>(&(mTerminalId.get())));
    }
    if (ret != kAUNoErr)
		MacOSError::throwMe(ret);
}


}	// end namespace CommonCriteria
}	// end namespace Security
