/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// devrandom - RNG operations based on /dev/random
//
#include <Security/devrandom.h>

using namespace UnixPlusPlus;


namespace Security {


//
// The common (shared) open file descriptor to /dev/random
//
ModuleNexus<FileDesc> DevRandomGenerator::mDevRandom;


//
// DevRandomGenerator objects immediately open their file descriptors
//
DevRandomGenerator::DevRandomGenerator(bool writable)
{
    FileDesc &fd = mDevRandom();
    if (!fd) {
        fd.open("/dev/random", writable ? O_RDWR : O_RDONLY);
    } else if (writable && !fd.isWritable()) {
        FileDesc newFd("/dev/random", O_RDWR);
        fd.close();
        fd = newFd;
    }
}


//
// Standard generate (directly from /dev/random)
//
void DevRandomGenerator::random(void *data, size_t length)
{
    mDevRandom().read(data, length);
}


//
// If you opened for writing, you add entropy to the global pool here
//
void DevRandomGenerator::addEntropy(const void *data, size_t length)
{
    mDevRandom().write(data, length);
}


}	// end namespace Security
