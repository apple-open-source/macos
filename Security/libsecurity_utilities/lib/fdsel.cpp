/*
 * Copyright (c) 2000-2001,2004 Apple Computer, Inc. All Rights Reserved.
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
// fdsel - select-style file descriptor set management
//
#include "fdsel.h"


namespace Security {
namespace UnixPlusPlus {


//
// Throw the bitvectors away on destruction
// 
FDSet::~FDSet()
{
    delete mBits;
    delete mUseBits;
}


//
// Given the old and desired new sizes (in fd_mask words), grow
// the bitvectors. New storage is zero filled. Note that we preserve
// the mUseBits vector, so this is safe to do during a post-select scan.
// This function cannot shrink the bitmaps.
//
void FDSet::grow(int oldWords, int newWords)
{
    assert(oldWords < newWords);
    grow(mBits, oldWords, newWords);
    grow(mUseBits, oldWords, newWords);
}

void FDSet::grow(fd_mask * &bits, int oldWords, int newWords)
{
    fd_mask *newBits = new fd_mask[newWords];
    memcpy(newBits, bits, oldWords * sizeof(fd_mask));
    memset(newBits + oldWords, 0, (newWords - oldWords) * sizeof(fd_mask));
    delete [] bits;
    bits = newBits;
}


//
// Set or clear a single bit in the map.
// No check for overflow is perfomed.
//
void FDSet::set(int fd, bool on)
{
    if (on) {
        FD_SET(fd, (fd_set *)mBits);
    } else {
        FD_CLR(fd, (fd_set *)mBits);
        FD_CLR(fd, (fd_set *)mUseBits);
    }
}


//
// Copy only the first words fd_mask words from mBits to mUseBits
// and return that for select(2) use.
//
fd_set *FDSet::make(int words)
{
    //@@@ if empty -> return NULL (but check caller for [] use)
    memcpy(mUseBits, mBits, words * sizeof(fd_mask));
    return (fd_set *)mUseBits;
}


}	// end namespace IPPlusPlus
}	// end namespace Security
