#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <architecture/byte_order.h>
#include <security_cdsa_utilities/cssmdb.h>
#include "SharedMemoryClient.h"
#include <string>
#include <security_utilities/crc.h>
#include <securityd_client/ssnotify.h>
#include <Security/SecKeychain.h>
#include <securityd_client/ssclient.h>
#include "dictionary.h"

using namespace Security;

//=================================================================================
//                          SharedMemoryClient
//=================================================================================

static std::string unixerrorstr(int errnum) {
    string errstr;
    char buf[1024];
    // might return ERANGE
    /* int rx = */ strerror_r(errnum, buf, sizeof(buf));
    errstr = string(buf);
    errstr += "(" + to_string(errnum) + ")";
    return errstr;
}

// rdar://81579429
// Always post a bogus keychain list changed event before trying to open the shared memory file, because securityd owns/creates those files.
// But there's a race condition: this notification has to get to the securityd process, and that process has to create the file before we
// try to open the file below. So we loop a few times trying to read the file.
static void TickleSharedMemoryServer(uid_t uid) {
    secdebug("MDSPRIVACY","[%03d] SharedMemoryClient tickling server", uid);

    NameValueDictionary nvd;

    Endian<pid_t> thePid = getpid();
    nvd.Insert (new NameValuePair (PID_KEY, CssmData (reinterpret_cast<void*>(&thePid), sizeof (pid_t))));

    // flatten the dictionary
    CssmData data;
    nvd.Export (data);

    /* enforce a maximum size of 16k for notifications */
    if (data.length() <= 16384) {
        SecurityServer::ClientSession cs (Allocator::standard(), Allocator::standard());
        cs.postNotification (SecurityServer::kNotificationDomainDatabase, kSecKeychainListChangedEvent, data);

        secinfo("MDSPRIVACY", "[%03d] SharedMemoryClient posted event %u", uid, (unsigned int) kSecKeychainListChangedEvent);
    }

    free (data.data ());
}

SharedMemoryClient::SharedMemoryClient (const char* segmentName, SegmentOffsetType segmentSize, uid_t uid)
{
	StLock<Mutex> _(mMutex);
	
	mSegmentName = segmentName;
	mSegmentSize = segmentSize;
    mSegment = (u_int8_t*) MAP_FAILED;
    mDataArea = mDataPtr = 0;
    mUID = uid;
    
    secdebug("MDSPRIVACY","[%03d] creating SharedMemoryClient with segmentName %s, size: %d", mUID, segmentName, segmentSize);

    if (segmentSize < sizeof(u_int32_t))
		return;

	// make the name
	int segmentDescriptor = -1;
	{
        std::string name(SharedMemoryCommon::SharedMemoryFilePath(mSegmentName.c_str(), mUID));

        int i = 0;
        do {
            // make a connection to the shared memory block
            segmentDescriptor = open (name.c_str(), O_RDONLY, S_IROTH);
            if (segmentDescriptor >= 0)
            {
                break;
            }

            secnotice("MDSPRIVACY","[%03d] SharedMemoryClient open of %s failed: %s", mUID, name.c_str(), unixerrorstr(errno).c_str());

            if (errno != ENOENT) { // hard error on opening the shared memory segment?
                // CssmError::throwMe (CSSM_ERRCODE_INTERNAL_ERROR);
                return;
            }

            if (i == 0) { // only tickle the first time through
                TickleSharedMemoryServer(uid);
            }

            i++;
        } while (i < 10 && 0 == usleep((1 << (i-1)) * 1000));
        // sum_(i=0…8)(2^i) is 511, so we'll wait a total of 511 miliseconds before giving up.
        // Note that we tried 10 times, but the waits are between the tries, not before the first and not after the last.
        
        if (segmentDescriptor < 0) // too many retries opening the shared memory segment
        {
            secnotice("MDSPRIVACY","[%03d] SharedMemoryClient open of %s failed repeatedly", mUID, name.c_str());
            // CssmError::throwMe (CSSM_ERRCODE_INTERNAL_ERROR);
            return;
        }
	}

    // check that the file size is large enough to support operations
    struct stat statResult = {};
    int result = fstat(segmentDescriptor, &statResult);
    if(result) {
        secdebug("MDSPRIVACY","[%03d] SharedMemoryClient fstat failed: %d/%s", mUID, result, unixerrorstr(errno).c_str());
        UnixError::throwMe(errno);
    }

    off_t sz = statResult.st_size;
    if(sz < sizeof(SegmentOffsetType)) {
        close(segmentDescriptor);
        return;
    }

    if(sz > 4*segmentSize) {
        // File is too ridiculously large. Quit.
        close(segmentDescriptor);
        return;
    }

	// map the segment into place
	mSegment = (u_int8_t*) mmap (NULL, segmentSize, PROT_READ, MAP_SHARED, segmentDescriptor, 0);
	close (segmentDescriptor);

	if (mSegment == MAP_FAILED)
	{
        secdebug("MDSPRIVACY","[%03d] SharedMemoryClient mmap failed: %d", mUID, errno);
		return;
	}
	
	mDataArea = mSegment + sizeof (SegmentOffsetType);
	mDataMax = mSegment + sz;
	mDataPtr = mDataArea + GetProducerCount ();
}



SharedMemoryClient::~SharedMemoryClient ()
{
    if (!uninitialized()) {
        StLock<Mutex> _(mMutex);
        munmap (mSegment, mSegmentSize);
    }
}


SegmentOffsetType SharedMemoryClient::GetProducerCount ()
{
    if (uninitialized()) {
        secdebug("MDSPRIVACY","[%03d] SharedMemoryClient::GetProducerCount uninitialized", mUID);
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
    if( ((u_int8_t*) (((u_int32_t*) mSegment) + 1)) > mDataMax) {
        // Check we can actually read this u_int32_t
        secdebug("MDSPRIVACY","[%03d] SharedMemoryClient::GetProducerCount uint > mDataMax", mUID);
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    }

	SegmentOffsetType offset = OSSwapBigToHostInt32 (*(u_int32_t*) mSegment);
    if (&mSegment[offset] >= mDataMax) {
        secdebug("MDSPRIVACY","[%03d] SharedMemoryClient::GetProducerCount offset > mDataMax", mUID);
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    }

    return offset;
}

void SharedMemoryClient::ReadData (void* buffer, SegmentOffsetType length)
{
    if (uninitialized()) {
        secdebug("MDSPRIVACY","[%03d] ReadData mSegment fail uninitialized: %p", mUID, mSegment);
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    }
    
	u_int8_t* bptr = (u_int8_t*) buffer;

	SegmentOffsetType bytesToEnd = (SegmentOffsetType)(mDataMax - mDataPtr);
	
	// figure out how many bytes we can read
	SegmentOffsetType bytesToRead = (length <= bytesToEnd) ? length : bytesToEnd;

	// move the first part of the data
	memcpy (bptr, mDataPtr, bytesToRead);
	bptr += bytesToRead;
	
	// see if we have anything else to read
	mDataPtr += bytesToRead;
	
	length -= bytesToRead;
	if (length != 0)
	{
		mDataPtr = mDataArea;
		memcpy(bptr, mDataPtr, length);
		mDataPtr += length;
	}
}



SegmentOffsetType SharedMemoryClient::ReadOffset()
{
	SegmentOffsetType offset;
	ReadData(&offset, sizeof(SegmentOffsetType));
	offset = OSSwapBigToHostInt32 (offset);
	return offset;
}



bool SharedMemoryClient::ReadMessage (void* message, SegmentOffsetType &length, UnavailableReason &ur)
{
	StLock<Mutex> _(mMutex);

    if (uninitialized()) {
		secdebug("MDSPRIVACY","[%03d] ReadMessage mSegment fail uninitialized: %p", mUID, mSegment);
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}

	ur = kURNone;
	
	size_t offset = mDataPtr - mDataArea;
	if (offset == GetProducerCount())
	{
        secdebug("MDSPRIVACY","[%03d] ReadMessage GetProducerCount()", mUID);
		ur = kURNoMessage;
		return false;
	}
	
	// get the length of the message in the buffer
	length = ReadOffset();
	
	// we have the possibility that data is correct, figure out where the data is actually located
	// get the length of the message stored there
	if (length == 0 || length >= kPoolAvailableForData)
	{
        secdebug("MDSPRIVACY","[%03d] ReadMessage length error: %d", mUID, length);
        ur = (length == 0) ? kURNoMessage : kURBufferCorrupt;

		// something's gone wrong, reset.
		mDataPtr = mDataArea + GetProducerCount ();
		return false;
	}
	
	// read the crc
	SegmentOffsetType crc = ReadOffset();

	// read the data into the buffer
	ReadData (message, length);
	
	// calculate the CRC
	SegmentOffsetType crc2 = CalculateCRC((u_int8_t*) message, length);
	if (crc != crc2)
	{
		ur = kURBufferCorrupt;
		mDataPtr = mDataArea + GetProducerCount ();
		return false;
	}

	return true;
}

//=================================================================================
//                          SharedMemoryCommon
//=================================================================================

std::string SharedMemoryCommon::SharedMemoryFilePath(const char *segmentName, uid_t uid) {
    std::string path;
    uid = SharedMemoryCommon::fixUID(uid);
    path = SharedMemoryCommon::kMDSMessagesDirectory;   // i.e. /private/var/db/mds/messages/
    if (uid != 0) {
        path += std::to_string(uid) + "/";              // e.g. /private/var/db/mds/messages/501/
    }

    path += SharedMemoryCommon::kUserPrefix;            // e.g. /var/db/mds/messages/se_
    path += segmentName;                                // e.g. /var/db/mds/messages/501/se_SecurityMessages
    return path;
}

std::string SharedMemoryCommon::notificationDescription(int domain, int event) {
    string domainstr, eventstr;

    switch (domain) {
        case Security::SecurityServer::kNotificationDomainAll:        domainstr = "all";      break;
        case Security::SecurityServer::kNotificationDomainDatabase:   domainstr = "database"; break;
        case Security::SecurityServer::kNotificationDomainPCSC:       domainstr = "pcsc";     break;
        case Security::SecurityServer::kNotificationDomainCDSA:       domainstr = "CDSA";     break;
        default:
            domainstr = "unknown";
            break;
    }

    switch (event) {
        case kSecLockEvent:                 eventstr = "lock";              break;
        case kSecUnlockEvent:               eventstr = "unlock";            break;
        case kSecAddEvent:                  eventstr = "add";               break;
        case kSecDeleteEvent:               eventstr = "delete";            break;
        case kSecUpdateEvent:               eventstr = "update";            break;
        case kSecPasswordChangedEvent:      eventstr = "passwordChange";    break;
        case kSecDefaultChangedEvent:       eventstr = "defaultChange";     break;
        case kSecDataAccessEvent:           eventstr = "dataAccess";        break;
        case kSecKeychainListChangedEvent:  eventstr = "listChange";        break;
        case kSecTrustSettingsChangedEvent: eventstr = "trustSettings";     break;
        default:
            domainstr = "unknown";
            break;
    }

    return "Domain: " + domainstr + ", Event: " + eventstr;
}
