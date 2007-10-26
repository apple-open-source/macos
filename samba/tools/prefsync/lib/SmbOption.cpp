/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include "macros.hpp"
#include "common.hpp"
#include "SmbConfig.hpp"
#include "SmbOption.hpp"

template <>
std::string property_convert<std::string>(CFPropertyListRef val)
{
    if (CFGetTypeID(val) != CFStringGetTypeID()) {
	throw std::runtime_error("unexpected non-CFString type");
    }

    return cfstring_convert((CFStringRef)val);
}

template <>
unsigned property_convert<unsigned>(CFPropertyListRef val)
{
    int nval;

    if (CFGetTypeID(val) != CFNumberGetTypeID()) {
	throw std::runtime_error("unexpected non-CFNumber type");
    }

    if (!CFNumberGetValue((CFNumberRef)val, kCFNumberIntType, &nval)) {
	throw std::range_error("unexpected integer representation");
    }

    if (nval < 0) {
	throw std::range_error("unexpected negative integer");
    }

    return (unsigned)nval;
}

template <>
bool property_convert<bool>(CFPropertyListRef val)
{
    if (CFGetTypeID(val) != CFBooleanGetTypeID()) {
	throw std::runtime_error("unexpected non-boolean type");
    }

    return cftype_equal<CFBooleanRef>(kCFBooleanTrue, (CFBooleanRef)val);
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
