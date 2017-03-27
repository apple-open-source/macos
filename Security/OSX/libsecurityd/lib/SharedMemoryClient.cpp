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
	int segmentDescriptor;
	{
        std::string name(SharedMemoryCommon::SharedMemoryFilePath(mSegmentName.c_str(), mUID));

		// make a connection to the shared memory block
		segmentDescriptor = open (name.c_str(), O_RDONLY, S_IROTH);
		if (segmentDescriptor < 0) // error on opening the shared memory segment?
		{
            secdebug("MDSPRIVACY","[%03d] SharedMemoryClient open of %s failed: %s", mUID, name.c_str(), unixerrorstr(errno).c_str());
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
