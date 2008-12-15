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

#include "SecureBufferAllocator.h"
#include "PIVUtilities.h"

template<size_t MAX_SIZE>
SecureBufferAllocator<MAX_SIZE>::~SecureBufferAllocator() {
	/* Clear out all buffers */
	for(size_t i = 0; i < buffers.size(); i++)
		secure_zero(buffers[i]);
}

template<size_t MAX_SIZE>
byte_string &SecureBufferAllocator<MAX_SIZE>::getBuffer() {
	byte_string &nextBuffer = buffers[nextFree];
	/* Calculate next available buffer */
	nextFree++;
	if(nextFree >= buffers.size())
		nextFree = 0;
	/* Clear out the buffer for use */
	secure_zero(nextBuffer);
	nextBuffer.clear();
	return nextBuffer;
}
