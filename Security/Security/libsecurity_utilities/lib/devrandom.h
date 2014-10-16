/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_DEVRANDOM
#define _H_DEVRANDOM

#include <security_utilities/utilities.h>
#include <security_utilities/unix++.h>
#include <security_utilities/globalizer.h>


namespace Security {


//
// This RNG uses /dev/random.
// It is not repeatable. AddEntropy() contributes random entropy to a global pool (only).
//
class DevRandomGenerator {
	struct Readonly : public UnixPlusPlus::FileDesc {
		Readonly() { open("/dev/random", O_RDONLY); }
	};
	
	struct Writable : public UnixPlusPlus::FileDesc {
		Writable() { open("/dev/random", O_RDWR); }
	};

public:
    DevRandomGenerator(bool writable = false);
    
    void random(void *data, size_t length);
    void addEntropy(const void *data, size_t length);

private:
    static ModuleNexus<Readonly> mReader;
	static ModuleNexus<Writable> mWriter;
};


}; 	// end namespace Security


#endif //_H_DEVRANDOM
