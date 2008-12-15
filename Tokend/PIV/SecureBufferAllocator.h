/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *
 *  @APPLE_LICENSE_HEADER_END@
 */

#ifndef SECUREBUFFERALLOCATOR_H
#define SECUREBUFFERALLOCATOR_H

#include "byte_string.h"
#include <tr1/array>

/* Utility class to store a fixed-size container of available buffers
 * Used to keepalive byte_strings for buffer usage while keeping contents secure
 * for re-use and on destruction.
 */
template<size_t MAX_SIZE>
class SecureBufferAllocator {
public:
	~SecureBufferAllocator();

	byte_string &getBuffer();
private:
	std::tr1::array<byte_string, MAX_SIZE> buffers;
	size_t nextFree;
};

#include "SecureBufferAllocator.inc"

#endif