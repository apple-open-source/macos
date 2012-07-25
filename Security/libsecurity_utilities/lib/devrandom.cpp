/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// devrandom - RNG operations based on /dev/random
//
#include <security_utilities/devrandom.h>
#include <security_utilities/logging.h>

using namespace UnixPlusPlus;


namespace Security {


//
// The common (shared) open file descriptor to /dev/random
//
ModuleNexus<DevRandomGenerator::Readonly> DevRandomGenerator::mReader;
ModuleNexus<DevRandomGenerator::Writable> DevRandomGenerator::mWriter;


//
// In the current implementation, opening the file descriptor is deferred.
//
DevRandomGenerator::DevRandomGenerator(bool writable)
{
}


//
// Standard generate (directly from /dev/random)
//
void DevRandomGenerator::random(void *data, size_t length)
{
    try {
		size_t bytesRead = mReader().read(data, length);
		if (bytesRead != length) {	// short read (shouldn't happen)
			Syslog::error("DevRandomGenerator: wanted %ld got %ld bytes",
				length, bytesRead);
			UnixError::throwMe(EIO);
		}
	} catch(const UnixError &uerr) {
		Syslog::error("DevRandomGenerator: error %d reading /dev/random",
			uerr.error);
		throw;
	}
}


//
// If you opened for writing, you add entropy to the global pool here
//
void DevRandomGenerator::addEntropy(const void *data, size_t length)
{
    if (mWriter().write(data, length) != length)
		UnixError::throwMe(EIO);	// short write (shouldn't happen)
}


}	// end namespace Security
