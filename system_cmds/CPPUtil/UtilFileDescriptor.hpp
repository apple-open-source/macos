//
//  UtilFileDescriptor.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilFileDescriptor_hpp
#define CPPUtil_UtilFileDescriptor_hpp

class FileDescriptor {
    protected:
	int _fd;

	// FD's aren't reference counted, we allow move semantics but
	// not copy semantics. Disable the copy constructor and copy
	// assignment.
	FileDescriptor(const FileDescriptor& that) = delete;
	FileDescriptor& operator=(const FileDescriptor& other) = delete;

    public:

	FileDescriptor() : _fd(-1)				{}
	FileDescriptor(int fd) : _fd(fd)			{}

	template <typename... Args>
	FileDescriptor(Args&& ... args) :
		_fd(open(static_cast<Args &&>(args)...))
	{
	}

	FileDescriptor (FileDescriptor&& rhs) noexcept :
		_fd(rhs._fd)
	{
		rhs._fd = -1;
	}

	~FileDescriptor()					{ close(); }

	FileDescriptor& operator=(int fd)			{ close(); _fd = fd; return *this; }
	FileDescriptor& operator=(FileDescriptor&& rhs)		{ std::swap(_fd, rhs._fd); return *this; }

	bool is_open() const					{ return _fd > -1 ? true : false; }
	void close()						{ if (is_open()) { ::close(_fd); _fd = -1; } }

	explicit operator bool() const				{ return is_open(); }
	operator int() const					{ return _fd; }
};


#endif
