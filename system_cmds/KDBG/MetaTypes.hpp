//
//  MetaTypes.hpp
//  KDBG
//
//  Created by James McIlree on 10/24/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

class Kernel32
{
    public:
	typedef uint32_t	ptr_t;

	enum { PTRMAX = UINT32_MAX };
	enum { is_64_bit = 0 };
};

class Kernel64
{
    public:
	typedef uint64_t	ptr_t;
	
	enum { PTRMAX = UINT64_MAX };
	enum { is_64_bit = 1 };
};
