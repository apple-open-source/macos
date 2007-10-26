/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/*
 *  PCSCDriverBundles.cpp
 *  SmartCardServices
 */

/*
	Creates a vector of driver bundle info structures from the hot-plug driver
	directory.

	Returns NULL on error and a pointer to an allocated HPDriver vector on
	success.  The caller must free the HPDriver with a call to HPDriversRelease().
 
	See http://developer.apple.com/documentation/CoreFoundation/Reference/CFArrayRef/index.html#//apple_ref/doc/uid/20001192
	for information about CFArrayApplyFunction
*/

#include "PCSCDriverBundles.h"
#include <security_utilities/debugging.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>
#include <map>

namespace PCSCD {

static const char *kPCSCLITE_HP_DROPDIR = "/usr/libexec/SmartCardServices/drivers/";
static const char *kENV_PCSC_DEBUG_DRIVER = "PCSC_DEBUG_DRIVER_DIR";	// environment var

DriverBundles::DriverBundles()
{
	// If debugging, look in build directory
#if !defined(NDEBUG)
	const char *envar = kENV_PCSC_DEBUG_DRIVER;
	if (envar)
		if (const char *envPath = getenv(envar))
		{
			// treat envPath as a classic colon-separated list of directories
			secdebug("pathlist", "%p configuring from env(\"%s\")", this, envar);
			while (const char *p = strchr(envPath, ':'))
			{
				addDirectory(string(envPath, p - envPath));
				envPath = p + 1;
			}
			addDirectory(envPath);
		}
#endif
	addDirectory(kPCSCLITE_HP_DROPDIR);
}

bool DriverBundles::find(PCSCD::Device &device)  const
{
	// Searches for a driver bundle that matches device. If found,
	// it sets the libpath for the device and returns true.

	ProductMatchMap matchingProducts;

	for (DriverBundles::const_iterator it=this->begin();it!=this->end();++it)
	{
		std::string name;
		const DriverBundle *bndl = static_cast<DriverBundle *>((*it).get());
		if (int32_t score = bndl->matches(device, name))
		{
			ProductMatchInfo *mi =  new ProductMatchInfo(bndl->path(),name);
			matchingProducts.push_back(make_pair(score, mi));
		}
	}
	
	if (matchingProducts.empty())
		return false;
	
	sort(matchingProducts.begin(), matchingProducts.end());
	const ProductMatchInfo *mi = (*matchingProducts.rbegin()).second;
	device.setName(mi->name());
	device.setPath(mi->path());
	// clean up
	for (ProductMatchMap::iterator it = matchingProducts.begin();it!=matchingProducts.end();++it)
		delete (*it).second;
	return true;
}

} // end namespace PCSCD
