/*
 * Copyright (c) 2004-2008,2013 Apple Inc. All Rights Reserved.
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
// token - internal representation of a (single distinct) hardware token
//
#include "token.h"
#include "tokendatabase.h"
#include "reader.h"
#include "notifications.h"
#include "child.h"
#include "server.h"
#include <securityd_client/dictionary.h>
#include <security_utilities/coderepository.h>
#include <security_utilities/logging.h>
#include <security_cdsa_client/mdsclient.h>
#include <SecurityTokend/SecTokend.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <grp.h>
#include <pwd.h>
#include <msgtracer_client.h>

using namespace MDSClient;


//
// SSID -> Token map
//
Token::SSIDMap Token::mSubservices;
// Make sure to always take mSSIDLock after we take the Token lock
// itself or own it's own.
Mutex Token::mSSIDLock;


//
// Token construction and destruction is trivial; the good stuff
// happens in insert() and remove() below.
//
Token::Token()
	: mFaulted(false), mTokend(NULL), mResetLevel(1)
{
	secinfo("token", "%p created", this);
}


Token::~Token()
{
	secinfo("token", "%p (%s:%d) destroyed",
		this, mGuid.toString().c_str(), mSubservice);
}


Reader &Token::reader() const
{
	return referent< ::Reader>();
}

TokenDaemon &Token::tokend()
{
	StLock<Mutex> _(*this);
	if (mFaulted)
		CssmError::throwMe(CSSM_ERRCODE_DEVICE_FAILED);
	if (mTokend)
		return *mTokend;
	else
		CssmError::throwMe(CSSM_ERRCODE_DEVICE_FAILED);
}


//
// We don't currently use a database handle to tokend.
// This is just to satisfy the TokenAcl.
//
GenericHandle Token::tokenHandle() const
{
	return noDb;	// we don't currently use tokend-side DbHandles
}


//
// Token is the SecurityServerAcl for the token
//
AclKind Token::aclKind() const
{
	return dbAcl;
}

Token &Token::token()
{
	return *this;
}


//
// Find Token by subservice id.
// Throws if ssid is invalid (i.e. always returns non-NULL)
//
RefPointer<Token> Token::find(uint32 ssid)
{
	StLock<Mutex> _(mSSIDLock);
	SSIDMap::const_iterator it = mSubservices.find(ssid);
	if (it == mSubservices.end())
		CssmError::throwMe(CSSMERR_CSSM_INVALID_SUBSERVICEID);
	else
		return it->second;
}


//
// We override getAcl to provide PIN state feedback
//
void Token::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	if (pinFromAclTag(tag, "?")) {	// read from tokend - do not cache
		AclEntryInfo *racls;
		token().tokend().getAcl(aclKind(), tokenHandle(), tag, count, racls);
		// make a chunk-copy because that's the contract we have with the caller
		acls = Allocator::standard().alloc<AclEntryInfo>(count * sizeof(AclEntryInfo));
		memcpy(acls, racls, count * sizeof(AclEntryInfo));
		ChunkCopyWalker copy;
		for (uint32 n = 0; n < count; n++)
			walk(copy, acls[n]);
		return;
	}

	TokenAcl::cssmGetAcl(tag, count, acls);
}


//
// Reset management.
// A Token has a "reset level", a number that is incremented whenever a token
// (hardware) reset is reported (as an error) by tokend. TokenAcls have their
// own matching level, which is that of the Token's when the ACL was last synchronized
// with tokend. Thus, incrementing the reset level invalidates all TokenAcls
// (without the need to enumerate them all).
// Note that a Token starts with a level of 1, while ACLs start at zero. This forces
// them to initially load their state from tokend.
//
Token::ResetGeneration Token::resetGeneration() const
{
	return mResetLevel;
}

void Token::resetAcls()
{
	CommonSet tmpCommons;
	{
		StLock<Mutex> _(*this);
		mResetLevel++;
		secinfo("token", "%p reset (level=%d, propagating to %ld common(s)",
			this, mResetLevel, mCommons.size());
		// Make a copy to avoid deadlock with TokenDbCommon lock
		tmpCommons = mCommons;
	}
	for (CommonSet::const_iterator it = tmpCommons.begin(); it != tmpCommons.end();)
		RefPointer<TokenDbCommon>(*it++)->resetAcls();
}

void Token::addCommon(TokenDbCommon &dbc)
{
	secinfo("token", "%p addCommon TokenDbCommon %p", this, &dbc);
	mCommons.insert(&dbc);
}

void Token::removeCommon(TokenDbCommon &dbc)
{
	secinfo("token", "%p removeCommon TokenDbCommon %p", this, &dbc);
	if (mCommons.find(&dbc) != mCommons.end())
		mCommons.erase(&dbc);
}


//
// Process the logical insertion of a Token into a Reader.
// From the client's point of view, this is where the CSSM subservice is created,
// characterized, and activated. From tokend's point of view, this is where
// we're analyzing the token, determine its characteristics, and get ready to
// use it.
//
void Token::insert(::Reader &slot, RefPointer<TokenDaemon> tokend)
{
	try {
		// this might take a while...
		Server::active().longTermActivity();
		referent(slot);
		mState = slot.pcscState();
		
		if (tokend == NULL) {
			// no pre-determined Tokend - search for one
			if (!(tokend = chooseTokend())) {
				secinfo("token", "%p no token daemons available - faulting this card", this);
				fault(false);	// throws
			}
		}

		// take Token lock and hold throughout insertion
		StLock<Mutex> _(*this);

		Syslog::debug("token inserted into reader %s", slot.name().c_str());
		secinfo("token", "%p begin insertion into slot %p (reader %s)",
			this, &slot, slot.name().c_str());
		
		// tell the tokend object to relay faults to us
		tokend->faultRelay(this);

		// locate or establish cache directories
		if (tokend->hasTokenUid()) {
			secinfo("token", "%p using %s (score=%d, uid=\"%s\")",
				this, tokend->bundlePath().c_str(), tokend->score(), tokend->tokenUid().c_str());
			mCache = new TokenCache::Token(reader().cache,
				tokend->bundleIdentifier() + ":" + tokend->tokenUid());
		} else {
			secinfo("token", "%p using %s (score=%d, temporary)",
				this, tokend->bundlePath().c_str(), tokend->score());
			mCache = new TokenCache::Token(reader().cache);
		}
		secinfo("token", "%p token cache at %s", this, mCache->root().c_str());
		
		// here's the primary parameters of the new subservice
		mGuid = gGuidAppleSdCSPDL;
		mSubservice = mCache->subservice();
		
		// establish work areas with tokend
		char mdsDirectory[PATH_MAX];
		char printName[PATH_MAX];
		tokend->establish(mGuid, mSubservice,
			(mCache->type() != TokenCache::Token::existing ? kSecTokendEstablishNewCache : 0) | kSecTokendEstablishMakeMDS,
			mCache->cachePath().c_str(), mCache->workPath().c_str(),
			mdsDirectory, printName);
		
		// establish print name
		if (mCache->type() == TokenCache::Token::existing) {
			mPrintName = mCache->printName();
			if (mPrintName.empty())
				mPrintName = printName;
		} else
			mPrintName = printName;
		if (mPrintName.empty()) {
			// last resort - new card and tokend didn't give us one
			snprintf(printName, sizeof(printName), "smart card #%d", mSubservice);
			mPrintName = printName;
		}
		if (mCache->type() != TokenCache::Token::existing)
			mCache->printName(mPrintName);		// store in cache

		// install MDS
		secinfo("token", "%p installing MDS from %s(%s)", this,
			tokend->bundlePath().c_str(),
			mdsDirectory[0] ? mdsDirectory : "ALL");
		string holdGuid = mGuid.toString();	// extend lifetime of std::string
		string holdTokenUid;
		if (tokend->hasTokenUid())
			holdTokenUid = tokend->tokenUid();
		string holdPrintName = this->printName();
		MDS_InstallDefaults mdsDefaults = {
			holdGuid.c_str(),
			mSubservice,
			holdTokenUid.c_str(),
			holdPrintName.c_str()
		};
		mds().install(&mdsDefaults,
			tokend->bundlePath().c_str(),
			mdsDirectory[0] ? mdsDirectory : NULL,
			NULL);

		{
			// commit to insertion
			StLock<Mutex> _(mSSIDLock);
			assert(mSubservices.find(mSubservice) == mSubservices.end());
			mSubservices.insert(make_pair(mSubservice, this));
		}

		// assign mTokend right before notification - mustn't be set if
		// anything goes wrong during insertion
		mTokend = tokend;

		notify(kNotificationCDSAInsertion);
		
		Syslog::notice("reader %s inserted token \"%s\" (%s) subservice %d using driver %s",
			slot.name().c_str(), mPrintName.c_str(),
			mTokend->hasTokenUid() ? mTokend->tokenUid().c_str() : "NO UID",
			mSubservice, mTokend->bundleIdentifier().c_str());
		secinfo("token", "%p inserted as %s:%d", this, mGuid.toString().c_str(), mSubservice);
	} catch (const CommonError &err) {
		Syslog::notice("token in reader %s cannot be used (error %d)", slot.name().c_str(), err.osStatus());
		secinfo("token", "exception during insertion processing");
		fault(false);
	} catch (...) {
		// exception thrown during insertion processing. Mark faulted
		Syslog::notice("token in reader %s cannot be used", slot.name().c_str());
		secinfo("token", "exception during insertion processing");
		fault(false);
	}
}


//
// Process the logical removal of a Token from a Reader.
// Most of the time, this is asynchronous - someone has yanked the physical
// token out of a physical slot, and we're left with changing our universe
// to conform to the new realities. Reality #1 is that we can't talk to the
// physical token anymore.
//
// Note that if we're in FAULT mode, there really isn't a TokenDaemon around
// to kick. We're just holding on to represent the fact that there *is* a (useless)
// token in the slot, and now it's been finally yanked. Good riddance.
//
void Token::remove()
{
	StLock<Mutex> _(*this);
	Syslog::notice("reader %s removed token \"%s\" (%s) subservice %d",
			reader().name().c_str(), mPrintName.c_str(),
			mTokend
				? (mTokend->hasTokenUid() ? mTokend->tokenUid().c_str() : "NO UID")
				: "NO tokend",
			mSubservice);
	secinfo("token", "%p begin removal from slot %p (reader %s)",
		this, &reader(), reader().name().c_str());
	if (mTokend)
		mTokend->faultRelay(NULL);		// unregister (no more faults, please)
	mds().uninstall(mGuid.toString().c_str(), mSubservice);
	secinfo("token", "%p mds uninstall complete", this);
	this->kill();
	secinfo("token", "%p kill complete", this);
	notify(kNotificationCDSARemoval);
	secinfo("token", "%p removal complete", this);
}


//
// Set the token to fault state.
// This essentially "cuts off" all operations on an inserted token and makes
// them fail. It also sends a FAULT notification via CSSM to any clients.
// Only one fault is actually processed; multiple calls are ignored.
//
// Note that a faulted token is not REMOVED; it's still physically present.
// No fault is declared when a token is actually removed.
//
void Token::fault(bool async)
{
	StLock<Mutex> _(*this);
	if (!mFaulted) {	// first one
		secinfo("token", "%p %s FAULT", this, async ? "ASYNCHRONOUS" : "SYNCHRONOUS");
		
		// mark faulted
		mFaulted = true;
		
		// send CDSA notification
		notify(kNotificationCDSAFailure);
		
		// cast off our TokenDaemon for good
//>>>		mTokend = NULL;
	}
	
	// if this is a synchronous fault, abort this operation now
	if (!async)
		CssmError::throwMe(CSSM_ERRCODE_DEVICE_FAILED);
}


void Token::relayFault(bool async)
{
	secinfo("token", "%p fault relayed from tokend", this);
	this->fault(async);
}


//
// This is the "kill" hook for Token as a Node<> object.
//
void Token::kill()
{
	// Avoid holding the lock across call to resetAcls
	// This can cause deadlock on card removal
	{
		StLock<Mutex> _(*this);
		if (mTokend)
		{
			mTokend = NULL;					// cast loose our tokend (if any)
			// Take us out of the map
			StLock<Mutex> _(mSSIDLock);
			SSIDMap::iterator it = mSubservices.find(mSubservice);
			assert(it != mSubservices.end() && it->second == this);
			if (it != mSubservices.end() && it->second == this)
				mSubservices.erase(it);
		}
	}
	
	resetAcls();					// release our TokenDbCommons
	PerGlobal::kill();				// generic action

}


//
// Send CDSA-layer notifications for this token.
// These events are usually received by CDSA plugins working with securityd.
//
void Token::notify(NotificationEvent event)
{
    NameValueDictionary nvd;
	CssmSubserviceUid ssuid(mGuid, NULL, h2n (mSubservice),
		h2n(CSSM_SERVICE_DL | CSSM_SERVICE_CSP));
	nvd.Insert(new NameValuePair(SSUID_KEY, CssmData::wrap(ssuid)));
    CssmData data;
    nvd.Export(data);

	// inject notification into Security event system
    Listener::notify(kNotificationDomainCDSA, event, data);
	
	// clean up
    free (data.data());
}

static void mt_log_ctk_tokend(const char *signature, const char *signature2)
{
    msgtracer_log_with_keys("com.apple.ctk.tokend", ASL_LEVEL_NOTICE,
                            "com.apple.message.signature", signature,
                            "com.apple.message.signature2", signature2,
                            "com.apple.message.summarize", "YES",
                            NULL);
}

//
// Choose a token daemon for our card.
//
// Right now, we probe tokends sequentially. If there are many tokends, it would be
// faster to launch them in parallel (relying on PCSC transactions to separate them);
// but it's not altogether clear whether this would slow things down on low-memory
// systems by forcing (excessive) swapping. There is room for future experimentation.
//
RefPointer<TokenDaemon> Token::chooseTokend()
{
	//@@@ CodeRepository should learn to update from disk changes to be re-usable
	CodeRepository<Bundle> candidates("Security/tokend", ".tokend", "TOKENDAEMONPATH", false);
	candidates.update();
	//@@@ we could sort by reverse "maxScore" and avoid launching those who won't cut it anyway...
	
	string chosenIdentifier;
	set<string> candidateIdentifiers;
	RefPointer<TokenDaemon> leader;
	for (CodeRepository<Bundle>::const_iterator it = candidates.begin();
			it != candidates.end(); it++) {
		RefPointer<Bundle> candidate = *it;
		try {
			// skip software token daemons - ineligible for automatic choosing
			if (CFTypeRef type = (*it)->infoPlistItem("TokendType"))
				if (CFEqual(type, CFSTR("software")))
					continue;

			// okay, launch it and let it try
			RefPointer<TokenDaemon> tokend = new TokenDaemon(candidate,
				reader().name(), reader().pcscState(), reader().cache);
			
			// add identifier to candidate names set
			candidateIdentifiers.insert(tokend->bundleIdentifier());

			if (tokend->state() == ServerChild::dead)	// ah well, this one's no good
				continue;
			
			// probe the (single) tokend
			if (!tokend->probe())		// non comprende...
				continue;

			// we got a contender!
			if (!leader || tokend->score() > leader->score()) {
				leader = tokend;		// a new front runner, he is...
				chosenIdentifier = leader->bundleIdentifier();
			}
		} catch (...) {
			secinfo("token", "exception setting up %s (moving on)", candidate->canonicalPath().c_str());
		}
	}

	// concatenate all candidate identifiers (sorted internally inside std::set)
	string identifiers;
	for (set<string>::const_iterator i = candidateIdentifiers.begin(), e = candidateIdentifiers.end(); i != e; ++i) {
		if (i != candidateIdentifiers.begin())
			identifiers.append(";");
		identifiers.append(*i);
	}
	mt_log_ctk_tokend(identifiers.c_str(), chosenIdentifier.c_str());

	return leader;
}


//
// Token::Access mediates calls through TokenDaemon to the actual daemon out there.
//
Token::Access::Access(Token &myToken)
	: token(myToken)
{
	mTokend = &token.tokend();	// throws if faulted or otherwise inappropriate
}

Token::Access::~Access()
{
}


//
// Debug dump support
//
#if defined(DEBUGDUMP)

void Token::dumpNode()
{
	PerGlobal::dumpNode();
	Debug::dump(" %s[%d] tokend=%p",
		mGuid.toString().c_str(), mSubservice, mTokend.get());
}

#endif //DEBUGDUMP
