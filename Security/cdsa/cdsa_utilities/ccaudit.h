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


#ifndef _H_CCAUDIT
#define _H_CCAUDIT

#include <Security/utility_config.h>
#include <bsm/audit.h>

namespace Security
{

namespace CommonCriteria
{

// for Tiger, this should be incorporated into Security's OSStatus range
enum ExternalErrors
{
	errNone = 0,
	errInvalidCredential = 1111,	// try to make easier to find in log
	errUserCanceled,
	errTooManyTries,
	errEndOfExternalErrors			// sentry/placeholder
};

class AuditMask
{
  public:
    AuditMask()				{ }
    AuditMask(const AuditMask &am)	{ set(am.get()); }
    AuditMask(const au_mask_t &am)	{ set(am); }
    ~AuditMask()			{ }

    void set(const au_mask_t &am)	{ set(am.am_success, am.am_failure); }
    void set(unsigned int s, unsigned int f)	{ mMask.am_success = s; mMask.am_failure = f; }
    const au_mask_t &get(void) const	{ return mMask; }

  private:
    au_mask_t mMask;
};

// For the most part, we won't have a machine ID to initialize the 
// au_tid_t's machine field.  There's no machine ID in the audit token,
// for example, since MIG is localhost-only.  
class TerminalId
{
  public:
    TerminalId()			{ }
    TerminalId(const TerminalId &t)	{ set(t.get()); }
    TerminalId(const au_tid_t &tid)	{ set(tid); }
    TerminalId(dev_t p, u_int32_t m)	{ port(p); machine(m); }
    ~TerminalId()			{ }

    void set(void);			// set using localhost
    void set(const au_tid_t &tid)	{ port(tid.port); machine(tid.machine); }
    void port(dev_t p)			{ mTid.port = p; }
    void machine(u_int32_t m)		{ mTid.machine = m; }
    const au_tid_t &get(void) const	{ return mTid; }

  private:
    au_tid_t mTid;
};

// audit session state for the current process; only used by Server
class AuditSession
{
  public:
    AuditSession()			{ }
    AuditSession(au_id_t auid, AuditMask &mask, au_asid_t sid, 
		 TerminalId &tid) 
	: mAuditId(auid), mEventMask(mask), mTerminalId(tid),
	  mSessionId(sid)		{ }
    ~AuditSession()			{ }

    // set audit info for this process in kernel
    void registerSession(void);

    void auditId(au_id_t auid)		{ mAuditId = auid; }
    void eventMask(AuditMask &mask)	{ mEventMask = mask; }
    void terminalId(TerminalId &tid)	{ mTerminalId = tid; }
    void sessionId(au_asid_t sid)	{ mSessionId = sid; }

    au_id_t auditId(void)		{ return mAuditId; }
    AuditMask &eventMask(void)		{ return mEventMask; }
    TerminalId &terminalId(void)	{ return mTerminalId; }
    au_asid_t sessionId(void)		{ return mSessionId; }

  private:
    au_id_t mAuditId;
    AuditMask mEventMask;
    TerminalId mTerminalId;
    au_asid_t mSessionId;
};

//
// For submitting audit records.  Not general-purpose: no ability to 
// submit arbitrary BSM tokens, for example.  However, the SecurityServer 
// has only limited auditing requirements under Common Criteria.  
//
class AuditRecord
{
  public:
    AuditRecord(const audit_token_t &auditToken)
	: mAuditId(auditToken.val[0]),
	  mRUid(auditToken.val[3]),
	  mRGid(auditToken.val[4]),
	  mEUid(auditToken.val[1]),
	  mEGid(auditToken.val[2]),
	  mPid(auditToken.val[5]),
	  mSessionId(auditToken.val[6]),
	  mTerminalId(auditToken.val[7], 0)	{ }
    ~AuditRecord()				{ }

    // returnCode == 0 --> success; nonzero returnCode --> failure
    void submit(const short event_code, const int returnCode, 
		const char *msg = NULL);

  private:
    au_id_t mAuditId;
    uid_t mRUid;
    gid_t mRGid;
    uid_t mEUid;
    gid_t mEGid;
    pid_t mPid;
    au_asid_t mSessionId;
    TerminalId mTerminalId;
};

} // end namespace CommonCriteria

} // end namespace Security

#endif	// _H_CCAUDIT
