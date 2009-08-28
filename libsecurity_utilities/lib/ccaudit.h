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

#include <security_utilities/utilities.h>
#include <mach/message.h>       // audit_token_t
#include <bsm/audit.h>          // au_tid_t, etc.
#include <bsm/audit_kevents.h>	// AUE_NULL

namespace Security
{

namespace CommonCriteria
{

class AuditToken;
    
/* 
 * For the most part, we won't have a machine ID to initialize the 
 * au_tid_t's machine field.  There's no machine ID in the audit token,
 * for example, since MIG is localhost-only.  
 */
class TerminalId: public PodWrapper<TerminalId, au_tid_t>
{
  public:
    TerminalId();
    TerminalId(const TerminalId &t)     { set(t); }
    TerminalId(const au_tid_t &tid)     { set(tid); }
    ~TerminalId()                       { }
    
    void set(const au_tid_t &tid)       { port = tid.port; machine = tid.machine; }
};

/* 
 * audit_token_t provides all the info required for Common Criteria-mandated
 * auditing.  It's defined in <mach/mach_types.defs>.  Its values are filled 
 * in by the kernel during a Mach RPC and it should be treated as read-only
 * thereafter.  
 */
class AuditToken
{
  public:
    AuditToken(const audit_token_t &token);
    ~AuditToken()					{ }
    
    uid_t auditId() const           { return mAuditId;          }
    uid_t euid() const              { return mEuid;             }
    gid_t egid() const              { return mEgid;             }
    uid_t ruid() const              { return mRuid;             }
    gid_t rgid() const              { return mRgid;             }
    pid_t pid() const               { return mPid;              }
    au_asid_t auditSession() const  { return mAuditSessionId;   }
    const au_tid_t &terminalId() const { return mTerminalId;	}
    
  private:
    uid_t mAuditId;
    uid_t mEuid;
    gid_t mEgid;
    uid_t mRuid;
    gid_t mRgid;
    pid_t mPid;						// of client
    au_asid_t mAuditSessionId;
    TerminalId mTerminalId;
};

// XXX/gh  3926739
//
// NB: Qualify all uses of these names with the namespace (CommonCriteria).  
// Existing source code already follows this convention.  
enum ExternalErrors
{
	errNone = 0,
	errInvalidCredential = 1111,	// try to make easier to find in log
	errUserCanceled,
	errTooManyTries,
    errAuthDenied,                  // "Auth" --> authorization; named to
                                    // avoid conflict with the C symbol
                                    // errAuthorizationDenied already in
                                    // use
	errEndOfExternalErrors			// sentry/placeholder
};

class AuditMask
{
  public:
    AuditMask(unsigned int s = AUE_NULL, unsigned int f = AUE_NULL)	
	{ 
		mMask.am_success = s; mMask.am_failure = f; 
	}
    ~AuditMask()						{ }
    const au_mask_t &get(void) const	{ return mMask; }

  private:
    au_mask_t mMask;
};

// audit session state for the current process; only used by Server
class AuditSession
{
  public:
    AuditSession(au_id_t auid, au_asid_t sid) 
	: mAuditId(auid), mSessionId(sid)	{ }
    ~AuditSession()						{ }

    // set audit info for this process in kernel
    void registerSession(void);

    void auditId(au_id_t auid)			{ mAuditId = auid;		}
    void eventMask(AuditMask &mask)		{ mEventMask = mask;	}
    void terminalId(TerminalId &tid)	{ mTerminalId = tid;	}
    void sessionId(au_asid_t sid)		{ mSessionId = sid;		}

    au_id_t auditId(void)				{ return mAuditId;		}
    AuditMask &eventMask(void)			{ return mEventMask;	}
    TerminalId &terminalId(void)		{ return mTerminalId;	}
    au_asid_t sessionId(void)			{ return mSessionId;	}

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
    AuditRecord(const AuditToken &auditToken)
		: mAuditToken(auditToken)	{ }
	AuditRecord(const audit_token_t &auditToken)
		: mAuditToken(auditToken)	{ }
    ~AuditRecord()					{ }

    // returnCode == 0 --> success; nonzero returnCode --> failure
    void submit(const short event_code, const int returnCode, 
		const char *msg = NULL);

  private:
    AuditToken mAuditToken;
};

} // end namespace CommonCriteria

} // end namespace Security

#endif	// _H_CCAUDIT
