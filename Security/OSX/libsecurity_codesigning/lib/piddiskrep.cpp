/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
#include "piddiskrep.h"
#include "sigblob.h"
#include <sys/param.h>
#include <sys/utsname.h>
#include <System/sys/codesign.h>
#include <libproc.h>
#include <xpc/xpc.h>

namespace Security {
namespace CodeSigning {
                
using namespace UnixPlusPlus;


void
PidDiskRep::setCredentials(const Security::CodeSigning::CodeDirectory *cd)
{
	// save the Info.plist slot
	if (cd->slotIsPresent(cdInfoSlot)) {
		mInfoPlistHash.take(makeCFData((*cd)[cdInfoSlot], cd->hashSize));
	}
}

void
PidDiskRep::fetchData(void)
{
	if (mDataFetched)	// once
		return;
	
	xpc_connection_t conn = xpc_connection_create("com.apple.CodeSigningHelper",
						      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
	xpc_connection_set_event_handler(conn, ^(xpc_object_t object){ });
	xpc_connection_resume(conn);
	
	xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
	assert(request != NULL);
	xpc_dictionary_set_string(request, "command", "fetchData");
	xpc_dictionary_set_int64(request, "pid", mPid);
	xpc_dictionary_set_data(request, "infohash", CFDataGetBytePtr(mInfoPlistHash), CFDataGetLength(mInfoPlistHash));
        
	xpc_object_t reply = xpc_connection_send_message_with_reply_sync(conn, request);
	if (reply && xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
		const void *data;
		size_t size;

		if (!mInfoPlist) {
			data = xpc_dictionary_get_data(reply, "infoPlist", &size);
			if (data && size > 0 && size < 50 * 1024)
				mInfoPlist.take(CFDataCreate(NULL, (const UInt8 *)data, (CFIndex)size));
		}
		if (!mBundleURL) {
			data = xpc_dictionary_get_data(reply, "bundleURL", &size);
			if (data && size > 0 && size < 50 * 1024)
				mBundleURL.take(CFURLCreateWithBytes(NULL, (const UInt8 *)data, (CFIndex)size, kCFStringEncodingUTF8, NULL));
		}
	}
	if (reply)
		xpc_release(reply);

	xpc_release(request);
	xpc_release(conn);
    
    if (!mBundleURL)
        MacOSError::throwMe(errSecCSNoSuchCode);
	
	mDataFetched = true;
}


PidDiskRep::PidDiskRep(pid_t pid, CFDataRef infoPlist)
	: mDataFetched(false)
{
        BlobCore header;
        CODESIGN_DISKREP_CREATE_KERNEL(this);
        
        mPid = pid;
        mInfoPlist = infoPlist;

//        fetchData();
    
        int rcent = ::csops(pid, CS_OPS_BLOB, &header, sizeof(header));
        if (rcent == 0)
                MacOSError::throwMe(errSecCSNoSuchCode);
        
        if (errno != ERANGE)
                UnixError::throwMe(errno);

        if (header.length() > 1024 * 1024)
                MacOSError::throwMe(errSecCSNoSuchCode);
        
        uint32_t bufferLen = (uint32_t)header.length();
        mBuffer = new uint8_t [bufferLen];
        
        UnixError::check(::csops(pid, CS_OPS_BLOB, mBuffer, bufferLen));

        const EmbeddedSignatureBlob *b = (const EmbeddedSignatureBlob *)mBuffer;
        if (!b->validateBlob(bufferLen))
                MacOSError::throwMe(errSecCSSignatureInvalid);
}

PidDiskRep::~PidDiskRep()
{
        if (mBuffer)
                delete [] mBuffer;
}


bool PidDiskRep::supportInfoPlist()
{
		fetchData();
        return mInfoPlist;
}


CFDataRef PidDiskRep::component(CodeDirectory::SpecialSlot slot)
{
	if (slot == cdInfoSlot) {
		fetchData();
		return mInfoPlist.retain();
	}

	EmbeddedSignatureBlob *b = (EmbeddedSignatureBlob *)this->blob();
	return b->component(slot);
}

CFDataRef PidDiskRep::identification()
{
        return NULL;
}


CFURLRef PidDiskRep::copyCanonicalPath()
{
	fetchData();
	return mBundleURL.retain();
}

string PidDiskRep::recommendedIdentifier(const SigningContext &)
{
	return string("pid") + to_string(mPid);
}

size_t PidDiskRep::signingLimit()
{
        return 0;
}

string PidDiskRep::format()
{
        return "pid diskrep";
}

UnixPlusPlus::FileDesc &PidDiskRep::fd()
{
        UnixError::throwMe(EINVAL);
}

string PidDiskRep::mainExecutablePath()
{
        char path[MAXPATHLEN * 2];
        if(::proc_pidpath(mPid, path, sizeof(path)) == 0)
		UnixError::throwMe(errno);

        return path;
}
                
                
} // end namespace CodeSigning
} // end namespace Security
