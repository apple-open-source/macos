/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

/*
 *  PCSCDriverBundles.h
 *  SmartCardServices
 */

#ifndef _H_XPCSCDRIVERBUNDLES
#define _H_XPCSCDRIVERBUNDLES

#include "PCSCDriverBundle.h"
#include "PCSCDevice.h"
#include <security_utilities/threading.h>
#include <security_utilities/coderepository.h>
#include <security_utilities/osxcode.h>
#include <set>
	
#if defined(__cplusplus)

namespace PCSCD {

class DriverBundles : public CodeRepository<DriverBundle>
{
	friend class DriverBundle;

public:
	DriverBundles();
	~DriverBundles() {}
	
	bool find(Device &device) const;
	
	// These are the things we need to know about which part of
	// bundle we are matched up with

	class ProductMatchInfo
	{
	public:
		ProductMatchInfo(std::string path, std::string name) : mPath(path), mName(name) {}
		
		std::string path() const { return mPath; }
		std::string name() const { return mName; }

	private:
		std::string mPath;
		std::string mName;
	};

	typedef std::vector< pair<int32_t, ProductMatchInfo * > > ProductMatchMap; 
};

} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif /* _H_XPCSCDRIVERBUNDLE */
