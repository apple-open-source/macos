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
// uniformrandom - uniformly distributed random number operators
//
#ifndef _H_UNIFORMRANDOM
#define _H_UNIFORMRANDOM

#include <Security/utilities.h>


namespace Security {


//
// Uniform binary blob generator.
// This operator deals exclusively in byte arrays.
//
template <class Generator>
class UniformRandomBlobs : public Generator {
public:
#if BUG_GCC
    void random(void *data, size_t length) { Generator::random(data, length); }
#else
    using Generator::random;
#endif

    template <class Object>
    void random(Object &obj)	{ random(&obj, sizeof(obj)); }
    
    void random(CssmData &data)	{ random(data.data(), data.length()); }
};


}; 	// end namespace Security


#endif //_H_UNIFORMRANDOM
