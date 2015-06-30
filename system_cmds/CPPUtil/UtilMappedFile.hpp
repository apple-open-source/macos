//
//  UtilMappedFile.h
//  CPPUtil
//
//  Created by James McIlree on 4/19/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilMappedFile__
#define __CPPUtil__UtilMappedFile__

class MappedFile {
    protected:
	unsigned char*	_address;
	size_t		_size;

    public:
	MappedFile(const char* path);
	~MappedFile();

	uint8_t* address()	{ return _address; }
	size_t size()		{ return _size; }

        bool mmap_failed() const	{ return _size > 0 && _address == nullptr; }
};

#endif /* defined(__CPPUtil__UtilMappedFile__) */
