/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CPluginConfig
 */

#ifndef __CPluginConfig_h__
#define __CPluginConfig_h__ 1

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include "PluginData.h"
#include "PrivateTypes.h"


#define	kConfigFilePath		"/Library/Preferences/DirectoryService/DirectoryService.plist"
#define kDefaultConfig		"<dict>\
	<key>Version</key>\
	<string>1.0</string>\
	<key>BSD Configuration Files</key>\
	<string>Inactive</string>\
</dict>"

#define	kActiveValue		"Active"
#define kInactiveValue		"Inactive"

class CPluginConfig
{
public:
					CPluginConfig		( void );
	virtual		   ~CPluginConfig		( void );

	sInt32			Initialize			( void );
	sInt32			SaveConfigData		( void );

	ePluginState	GetPluginState		( const char *inPluginName );
	sInt32			SetPluginState		( const char *inPluginName, const ePluginState inPluginState );

protected:

private:
	//CFDataRef			fDataRef;
	CFPropertyListRef	fPlistRef;
	CFMutableDictionaryRef	fDictRef;
};

#endif
