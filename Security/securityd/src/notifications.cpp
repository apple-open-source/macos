/*
 * Copyright (c) 2000-2004,2006,2008 Apple Inc. All Rights Reserved.
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
// notifications - handling of securityd-gated notification messages
//
#include <notify.h>
#include <sys/sysctl.h>

#include "notifications.h"
#include "server.h"
#include "connection.h"
#include "dictionary.h"
#include "SharedMemoryClient.h"

#include <securityd_client/ucspNotify.h>
#include <security_utilities/casts.h>

#include <Security/SecKeychain.h>
#include <Security/SecItemInternal.h>

Listener::ListenerMap& Listener::listeners = *(new Listener::ListenerMap);
Mutex Listener::setLock(Mutex::recursive);


//
// Listener basics
//
Listener::Listener(NotificationDomain dom, NotificationMask evs, mach_port_t port)
	: domain(dom), events(evs)
{
	assert(events);		// what's the point?
	
    // register in listener set
    StLock<Mutex> _(setLock);
    listeners.insert(ListenerMap::value_type(port, this));
	
	secinfo("notify", "%p created for domain 0x%x events 0x%x port %d",
		this, dom, evs, port);
}

Listener::~Listener()
{
    secinfo("notify", "%p destroyed", this);
}


//
// Send a notification to all registered listeners
//
void Listener::notify(NotificationDomain domain,
	NotificationEvent event, const CssmData &data)
{
	RefPointer<Notification> message = new Notification(domain, event, 0, data);
	StLock<Mutex> _(setLock);
	sendNotification(message);
}

void Listener::notify(NotificationDomain domain,
	NotificationEvent event, uint32 sequence, const CssmData &data, audit_token_t auditToken)
{
	Connection &current = Server::active().connection();
	RefPointer<Notification> message = new Notification(domain, event, sequence, data);
	if (current.inSequence(message)) {
		StLock<Mutex> _(setLock);

		// This is a total layer violation, but no better place to put it
		uid_t uid = audit_token_to_euid(auditToken);
		gid_t gid = audit_token_to_egid(auditToken);
		SharedMemoryListener::createDefaultSharedMemoryListener(uid, gid);

		sendNotification(message);
		while (RefPointer<Notification> next = current.popNotification())
			sendNotification(next);
	}
}

void Listener::sendNotification(Notification *message)
{
    secdebug("MDSPRIVACY","Listener::sendNotification for uid/euid: %d/%d", getuid(), geteuid());

    for (ListenerMap::const_iterator it = listeners.begin();
            it != listeners.end(); it++) {
		Listener *listener = it->second;
		if (listener->domain == kNotificationDomainAll ||
            (message->domain == listener->domain && listener->wants(message->event)))
			listener->notifyMe(message);
	}
}


//
// Handle a port death or deallocation by removing all Listeners using that port.
// Returns true iff we had one.
//
bool Listener::remove(Port port)
{
    typedef ListenerMap::iterator Iterator;
    StLock<Mutex> _(setLock);
    pair<Iterator, Iterator> range = listeners.equal_range(port);
    if (range.first == range.second)
        return false;	// not one of ours

	assert(range.first != listeners.end());
	secinfo("notify", "remove port %d", port.port());
#if !defined(NDEBUG)
    for (Iterator it = range.first; it != range.second; it++) {
		assert(it->first == port);
		secinfo("notify", "%p listener removed", it->second.get());
	}
#endif //NDEBUG
    listeners.erase(range.first, range.second);
	port.destroy();
    return true;	// got it
}


//
// Notification message objects
//
Listener::Notification::Notification(NotificationDomain inDomain,
	NotificationEvent inEvent, uint32 seq, const CssmData &inData)
	: domain(inDomain), event(inEvent), sequence(seq), data(Allocator::standard(), inData)
{
	secinfo("notify", "%p notification created domain 0x%x event %d seq %d",
		this, domain, event, sequence);
}

Listener::Notification::~Notification()
{
	secinfo("notify", "%p notification done domain 0x%x event %d seq %d",
		this, domain, event, sequence);
}

std::string Listener::Notification::description() const {
    return SharedMemoryCommon::notificationDescription(domain, event) +
        ", Seq: " + std::to_string(sequence) + ", Data: " + std::to_string(this->size());
}

//
// Jitter buffering
//
bool Listener::JitterBuffer::inSequence(Notification *message)
{
	if (message->sequence == mNotifyLast + 1) {	// next in sequence
		mNotifyLast++;			// record next sequence
		return true;			// go ahead
	} else {
		secinfo("notify-jit", "%p out of sequence (last %d got %d); buffering",
			message, mNotifyLast, message->sequence);
		mBuffer[message->sequence] = message;	// save for later
		return false;			// hold your fire
	}
}

RefPointer<Listener::Notification> Listener::JitterBuffer::popNotification()
{
	JBuffer::iterator it = mBuffer.find(mNotifyLast + 1);	// have next message?
	if (it == mBuffer.end())
		return NULL;			// nothing here
	else {
		RefPointer<Notification> result = it->second;	// save value
		mBuffer.erase(it);		// remove from buffer
		secinfo("notify-jit", "%p retrieved from jitter buffer", result.get());
		return result;			// return it
	}
}

bool Listener::testPredicate(const std::function<bool(const Listener& listener)> test) {
    StLock<Mutex> _(setLock);
    for (ListenerMap::const_iterator it = listeners.begin(); it != listeners.end(); it++) {
        if (test(*(it->second)))
            return true;
    }
    return false;
}

/*
 * Shared memory listener
 */


SharedMemoryListener::SharedMemoryListener(const char* segmentName, SegmentOffsetType segmentSize, uid_t uid, gid_t gid) :
	Listener (kNotificationDomainAll, kNotificationAllEvents),
	SharedMemoryServer (segmentName, segmentSize, uid, gid),
	mActive (false)
{
	if (segmentName == NULL)
	{
		secerror("Attempted to start securityd with a NULL segmentName");
        abort();
	}
}

SharedMemoryListener::~SharedMemoryListener ()
{
}

// Look for a listener for a given user ID
bool SharedMemoryListener::findUID(uid_t uid) {
    return Listener::testPredicate([uid](const Listener& listener) -> bool {
        try {
            // There may be elements in the map that are not SharedMemoryListeners
            const SharedMemoryListener& smlListener = dynamic_cast<const SharedMemoryListener&>(listener);
            if (smlListener.mUID == uid)
                return true;
        }
        catch (...) {
            return false;
        }
        return false;
    }
    );
    return false;
}

void SharedMemoryListener::createDefaultSharedMemoryListener(uid_t uid, gid_t gid) {
    uid_t fuid = SharedMemoryCommon::fixUID(uid);
    if (fuid != 0) { // already created when securityd started up
        if (!SharedMemoryListener::findUID(fuid)) {
            secdebug("MDSPRIVACY","creating SharedMemoryListener for uid/gid: %d/%d", fuid, gid);
            // A side effect of creation of a SharedMemoryListener is addition to the ListenerMap
#ifndef __clang_analyzer__
            /* __unused auto sml = */ new SharedMemoryListener(SharedMemoryCommon::kDefaultSecurityMessagesName, kSharedMemoryPoolSize, uid, gid);
#endif  // __clang_analyzer__
        }
    }
}

// Simpler local version of PrimaryKeyImpl::getUInt32
uint32 SharedMemoryListener::getRecordType(const CssmData& val) const {
    if (val.Length < sizeof(uint32))
        return 0;               // Not really but good enough for here

    const uint8 *pv = val.Data;
    // @@@ Assumes data written in big endian.
    uint32 value = (pv[0] << 24) + (pv[1] << 16) + (pv[2] << 8) + pv[3];
    return value;
}

bool SharedMemoryListener::isTrustEvent(Notification *notification) {
    bool trustEvent = false;

    switch (notification->event) {
        case kSecDefaultChangedEvent:
        case kSecKeychainListChangedEvent:
        case kSecTrustSettingsChangedEvent:
            trustEvent = true;
            break;
        case kSecAddEvent:
        case kSecDeleteEvent:
        case kSecUpdateEvent:
            {
                NameValueDictionary dictionary (notification->data);
                const NameValuePair *item = dictionary.FindByName(ITEM_KEY);
                if (item && (CSSM_DB_RECORDTYPE)getRecordType(item->Value()) == CSSM_DL_DB_RECORD_X509_CERTIFICATE) {
                    trustEvent = true;
                }
            }
            break;
        default:
            break;
    }

    if (trustEvent) {
        uint32_t result = notify_post(kSecServerCertificateTrustNotification);
        if (result != NOTIFY_STATUS_OK) {
            secdebug("MDSPRIVACY","Certificate trust event notification failed: %d", result);
        }
    }

    secdebug("MDSPRIVACY","[%03d] Event is %s trust event", mUID, trustEvent?"a":"not a");
    return trustEvent;
}

bool SharedMemoryListener::needsPrivacyFilter(Notification *notification) {
    if (notification->domain == kNotificationDomainPCSC || notification->domain == kNotificationDomainCDSA)
        return false;

    // kNotificationDomainDatabase		= 1, // something happened to a database (aka keychain)
    switch (notification->event) {
    case kSecLockEvent:             // kNotificationEventLocked
    case kSecUnlockEvent:           // kNotificationEventUnlocked
    case kSecPasswordChangedEvent:  // kNotificationEventPassphraseChanged
    case kSecDefaultChangedEvent:
    case kSecDataAccessEvent:
    case kSecKeychainListChangedEvent:
    case kSecTrustSettingsChangedEvent:
        return false;
    case kSecAddEvent:
    case kSecDeleteEvent:
    case kSecUpdateEvent:
        break;
    }

    secdebug("MDSPRIVACY","[%03d] Evaluating event %s", mUID, notification->description().c_str());

    NameValueDictionary dictionary (notification->data);
    const NameValuePair *item = dictionary.FindByName(ITEM_KEY);

    // If we don't have an item, there is nothing to filter
    if (!item) {
        secdebug("MDSPRIVACY","[%03d] Item event did not contain an item", mUID);
        return false;
    }

    pid_t thisPid = 0;
    const NameValuePair *pidRef = dictionary.FindByName(PID_KEY);
    if (pidRef != 0) {
        thisPid = n2h(*reinterpret_cast<pid_t*>(pidRef->Value().data()));
    }

    uid_t out_euid = 0;
    int rx = SharedMemoryListener::get_process_euid(thisPid, out_euid);
    if (rx != 0) {
        secdebug("MDSPRIVACY","[%03d] get_process_euid failed (rx=%d), filtering out item", mUID, rx);
        return true;
    }

    if (out_euid == mUID) {
        return false;       // Listener owns this item, so no filtering
    }

    // Allow processes running as root to pass through certificates
    if (out_euid == 0) {
        CSSM_DB_RECORDTYPE recordType = getRecordType(item->Value());
        if (recordType == CSSM_DL_DB_RECORD_X509_CERTIFICATE) {
            return false;
        }
    }

    secdebug("MDSPRIVACY","[%03d] Filtering event %s", mUID, notification->description().c_str());
    return true;
}

const double kServerWait = 0.005; // time in seconds before clients will be notified that data is available

void SharedMemoryListener::notifyMe(Notification* notification)
{
    const void* data = notification->data.data();
    size_t length = notification->data.length();
    /* enforce a maximum size of 16k for notifications */
    if (length > 16384) return;

    isTrustEvent(notification);
    if (needsPrivacyFilter(notification)) {
        return; // just drop it
    }

    secdebug("MDSPRIVACY","[%03d] WriteMessage event %s", mUID, notification->description().c_str());

    WriteMessage (notification->domain, notification->event, data, int_cast<size_t, UInt32>(length));

    if (!mActive)
    {
        Server::active().setTimer (this, Time::Interval(kServerWait));
        mActive = true;
    }
}

void SharedMemoryListener::action ()
{
	secinfo("notify", "Posted notification to clients.");
    secdebug("MDSPRIVACY","[%03d] Posted notification to clients", mUID);
	notify_post (mSegmentName.c_str ());
	mActive = false;
}

int SharedMemoryListener::get_process_euid(pid_t pid, uid_t& out_euid) {
    struct kinfo_proc proc_info = {};
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    size_t len = sizeof(struct kinfo_proc);
    int ret = sysctl(mib, (sizeof(mib)/sizeof(int)), &proc_info, &len, NULL, 0);

    out_euid = -1;
    if (ret == 0) {
        out_euid = proc_info.kp_eproc.e_ucred.cr_uid;
    }
    return ret;
}
