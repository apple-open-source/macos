/*
 *  Copyright (c) 2008 Apple Inc. All Rights Reserved.
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

#ifndef PIV_UTILITIES_H
#define PIV_UTILITIES_H

#include "byte_string.h"
#include <algorithm>

template<typename T>
inline void secure_zero(T &l) {
	std::fill(l.begin(), l.end(), typename T::value_type());
}

template<typename T>
inline void secure_erase(T &data, const typename T::iterator &first, const typename T::iterator &last) {
	/* Partly borrowing from alg used by normal 'erase' */
	typename T::iterator newEnd(std::copy(last, data.end(), first));
	// Filling w/ defaults to null values out
	std::fill(newEnd, data.end(), typename T::value_type());
	data.erase(newEnd, data.end());
}

template<typename T>
inline void secure_resize(T &data, const size_t newSize) {
	// Simple case where no re-allocation occurs
	if(data.capacity() >= newSize) {
		data.resize(newSize);
		return;
	}
	// Re-allocation will occur, need to use temporary buffer...
	T temporary(data);
	secure_zero(data);
	data.resize(newSize);
	copy(temporary.begin(), temporary.end(), data.begin());
	secure_zero(temporary);
}
	
#endif
