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
// fdsel - select-style file descriptor set management
//
#ifndef _H_FDSEL
#define _H_FDSEL

#include <Security/utilities.h>
#include <sys/types.h>
#include <Security/debugging.h>


namespace Security {
namespace UnixPlusPlus {


//
// An FDSet object maintains a single select(2) compatible bitmap.
// Size is implicitly kept by the caller (who needs to call grow() as
// needed, starting at zero). As long as this is done correctly, we are
// not bound by the FD_SETSIZE limit.
// An FDSet can self-copy for select(2) use; after that, the [] operator
// investigates the copy.
//
// Why are we using the FD_* macros even though we know these
// are fd_mask arrays? Some implementations use optimized assembly
// for these operations, and we want to pick those up.
//
// This whole mess is completely UNIX specific. If your system has
// the poll(2) system call, ditch this and use it.
//
class FDSet {
public:
    FDSet() : mBits(NULL), mUseBits(NULL) { }
    ~FDSet();

    void grow(int oldWords, int newWords);
    void set(int fd, bool on);
    
    fd_set *make(int words);
    bool operator [] (int fd) const	{ return FD_ISSET(fd, (fd_set *)mUseBits); }
    
    inline static int words(int fd)	{ return (fd - 1) / NFDBITS + 1; }

private:
    fd_mask *mBits;					// base bits
    fd_mask *mUseBits;				// mutable copy for select(2)
    
    void grow(fd_mask * &bits, int oldWords, int newWords);
};


}	// end namespace UnixPlusPlus
}	// end namespace Security


#endif //_H_FDSEL
