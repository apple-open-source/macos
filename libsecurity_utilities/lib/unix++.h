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
// unix++ - C++ layer for basic UNIX facilities
//
#ifndef _H_UNIXPLUSPLUS
#define _H_UNIXPLUSPLUS

#include <security_utilities/utilities.h>
#include <security_utilities/errors.h>
#include <security_utilities/timeflow.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <cstdio>
#include <cstdarg>
#include <map>


namespace Security {
namespace UnixPlusPlus {


//
// Check system call return and throw on error
//
template <class Result>
inline void checkError(Result result)
{
	if (result == Result(-1))
		UnixError::throwMe();
}


//
// A UNIX standard 'struct iovec' wrapped
//
class IOVec : public iovec {
public:
	IOVec() { }
	IOVec(const void *data, size_t length)		{ set(data, length); }
	IOVec(void *data, size_t length)			{ set(data, length); }
	
	void set(const void *data, size_t length)
	{ iov_base = reinterpret_cast<char *>(const_cast<void *>(data)); iov_len = length; }
	
	// data-oid methods
	void *data() const			{ return iov_base; }
	size_t length() const		{ return iov_len; }
};


//
// Generic file descriptors
//
class FileDesc {
protected:
    static const int invalidFd = -1;

    void setFd(int fd)					{ mFd = fd; mAtEnd = false; }
    void checkSetFd(int fd)				{ checkError(fd); mFd = fd; mAtEnd = false; }
    
public:
    FileDesc() : mFd(invalidFd), mAtEnd(false) { }
    FileDesc(int fd) : mFd(fd), mAtEnd(false) { }
    
    // implicit file system open() construction
    FileDesc(const char *path, int flag = O_RDONLY, mode_t mode = 0666) : mFd(invalidFd)
    { open(path, flag, mode); }
	FileDesc(const std::string &path, int flag = O_RDONLY, mode_t mode = 0666) : mFd(invalidFd)
	{ open(path.c_str(), flag, mode); }
    
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
	
	size_t readAll(void *addr, size_t length);
	size_t readAll(std::string &content);
	void writeAll(const void *addr, size_t length);
	void writeAll(const std::string &s) { writeAll(s.data(), s.length()); }
    
    // more convenient I/O
    template <class T> size_t read(T &obj) { return read(&obj, sizeof(obj)); }
    template <class T> size_t write(const T &obj) { return write(&obj, sizeof(obj)); }
    
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
    
    int openMode() const	{ return flags() & O_ACCMODE; }
    bool isWritable() const	{ return openMode() != O_RDONLY; }
    bool isReadable() const	{ return openMode() != O_WRONLY; }
	
	// lock support (fcntl style)
	struct Pos {
		Pos(off_t s = 0, int wh = SEEK_SET, off_t siz = 0)
			: start(s), size(siz), whence(wh) { }

		off_t start;
		off_t size;
		int whence;
	};
	static Pos lockAll()	{ return Pos(0, SEEK_SET, 0); }
	
	void lock(struct flock &args);	// raw form (fill in yourself)
	
	void lock(int type = F_WRLCK, const Pos &pos = lockAll());
	bool tryLock(int type = F_WRLCK, const Pos &pos = lockAll());
	void unlock(const Pos &pos = lockAll()) { lock(F_UNLCK, pos); }
    
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

private:
	struct LockArgs : public flock {
		LockArgs(int type, const Pos &pos)
		{ l_start = pos.start; l_len = pos.size; l_type = type; l_whence = pos.whence; }
		IFDEBUG(void debug(int fd, const char *what));
	};
    
protected:
    bool mAtEnd;			// end-of-data indicator (after zero read)
};


//
// A (plain) FileDesc that auto-closes
//
class AutoFileDesc : public FileDesc {
public:
    AutoFileDesc() { }
    AutoFileDesc(int fd) : FileDesc(fd) { }
    
    AutoFileDesc(const char *path, int flag = O_RDONLY, mode_t mode = 0666)
        : FileDesc(path, flag, mode) { }
	AutoFileDesc(const std::string &path, int flag = O_RDONLY, mode_t mode = 0666)
		: FileDesc(path, flag, mode) { }

    ~AutoFileDesc()		{ close(); }
};


//
// Signal sets
//
class SigSet {
public:
	SigSet() { sigemptyset(&mValue); }
	SigSet(const sigset_t &s) : mValue(s) { }

	SigSet &operator += (int sig)
		{ sigaddset(&mValue, sig); return *this; }
	SigSet &operator -= (int sig)
		{ sigdelset(&mValue, sig); return *this; }
	
	bool contains(int sig)
		{ return sigismember(&mValue, sig); }
	
	sigset_t &value()			{ return mValue; }
	operator sigset_t () const	{ return mValue; }
	
private:
	sigset_t mValue;
};

SigSet sigMask(SigSet set, int how = SIG_SETMASK);


//
// A ForkMonitor determines whether the current thread is a (fork) child of
// the thread that last checked it. Essentially, it checks for pid changes.
//
class StaticForkMonitor {
public:
	bool operator () () const
	{
		if (mLastPid == 0) {
			mLastPid = getpid();
			return false;
		} else if (getpid() != mLastPid) {
			mLastPid = getpid();
			return true;
		}
		return false;
	}
	
protected:
	mutable pid_t mLastPid;
};

class ForkMonitor : public StaticForkMonitor {
public:
	ForkMonitor()		{ mLastPid = getpid(); }
};


//
// Miscellaneous functions to aid the intrepid UNIX hacker
//
void makedir(const char *path, int flags, mode_t mode = 0777);

int ffprintf(const char *path, int flags, mode_t mode, const char *format, ...);
int ffscanf(const char *path, const char *format, ...);


}	// end namespace UnixPlusPlus
}	// end namespace Security


#endif //_H_UNIXPLUSPLUS
