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
// unix++ - C++ layer for basic UNIX facilities
//
#ifndef _H_UNIXPLUSPLUS
#define _H_UNIXPLUSPLUS

#include <Security/utilities.h>
#include "timeflow.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdarg>
#include <map>


namespace Security {
namespace UnixPlusPlus {


//
// Generic file descriptors
//
class FileDesc {
protected:
    static const int invalidFd = -1;

    void setFd(int fd)					{ mFd = fd; mAtEnd = false; }
    static void checkError(int result)	{ if (result == -1) UnixError::throwMe(); }
    void checkSetFd(int fd)				{ checkError(fd); mFd = fd; mAtEnd = false; }
    
public:
    FileDesc() : mFd(invalidFd), mAtEnd(false) { }
    FileDesc(int fd) : mFd(fd), mAtEnd(false) { }
    
    // implicit file system open() construction
    FileDesc(const char *path, int flag = O_RDONLY, mode_t mode = 0666) : mFd(invalidFd)
    { open(path, flag, mode); }
    
    // assignment
    FileDesc &operator = (int fd)		{ mFd = fd; mAtEnd = false; return *this; }
    FileDesc &operator = (const FileDesc &fd) { mFd = fd.mFd; mAtEnd = fd.mAtEnd; return *this; }
    
    bool isOpen() const			{ return mFd != invalidFd; }
    operator bool() const 		{ return isOpen(); }
    int fd() const				{ return mFd; }
    operator int() const		{ return fd(); }
    
    void clear()				{ mFd = invalidFd; }
    void close();				// close and clear
    
    void open(const char *path, int flag = O_RDONLY, mode_t mode = 0666);
    
    // basic I/O: this defines the "Filedescoid" pseudo-type
    size_t read(void *addr, size_t length);
    size_t write(const void *addr, size_t length);
    bool atEnd() const			{ return mAtEnd; }	// valid after zero-length read only

    // seeking
    off_t seek(off_t position, int whence = SEEK_SET);
    
    // mapping support
    void *mmap(int prot = PROT_READ, size_t length = 0, int flags = MAP_FILE, 
        off_t offset = 0, void *addr = NULL);
    
    // fcntl support
    int fcntl(int cmd, int arg = 0) const;
    int fcntl(int cmd, void *arg) const;
    int flags() const;
    void flags(int flags) const;
    void setFlag(int flag, bool on = true) const;
    void clearFlag(int flag) const	{ setFlag(flag, false); }
    
    // ioctl support
    int ioctl(int cmd, void *arg) const;
    template <class Arg> Arg iocget(int cmd) const 
        { Arg arg; ioctl(cmd, &arg); return arg; }
    template <class Arg> void iocget(int cmd, Arg &arg) const
        { ioctl(cmd, &arg); }
    template <class Arg> void iocset(int cmd, const Arg &arg)
        { ioctl(cmd, const_cast<Arg *>(&arg)); }
        
    // stat-related utilities. @@@ should cache??
    typedef struct stat UnixStat;
    void fstat(UnixStat &st) const;
    size_t fileSize() const;
    
    // stdio interactions
    FILE *fdopen(const char *mode = NULL);	// fdopen(3)

private:
    int mFd;				// UNIX file descriptor
    
protected:
    bool mAtEnd;			// end-of-data indicator (after zero read)
};


}	// end namespace UnixPlusPlus
}	// end namespace Security


#endif //_H_UNIXPLUSPLUS
