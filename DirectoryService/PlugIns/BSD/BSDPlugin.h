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

#ifndef _BSDPLUGIN_H
#define _BSDPLUGIN_H

#include "BaseDirectoryPlugin.h"

class BSDPlugin : public BaseDirectoryPlugin
{
	public:
								BSDPlugin				( FourCharCode inSig, const char *inName );
		virtual					~BSDPlugin				( void );

		virtual SInt32			Initialize				( void );
		virtual SInt32			SetPluginState			( const UInt32 inState );
		virtual SInt32			PeriodicTask			( void );
	
	protected:
		virtual CFDataRef		CopyConfiguration		( void );
		virtual bool			NewConfiguration		( const char *inData, UInt32 inLength );
		virtual bool			CheckConfiguration		( const char *inData, UInt32 inLength );
		virtual tDirStatus			HandleCustomCall		( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData );
		virtual bool			IsConfigureNodeName		( CFStringRef inNodeName );
		virtual BDPIVirtualNode	*CreateNodeForPath		( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID );
	
	private:
		CFStringRef		fFlatFilesNode;
		CFStringRef		fNISDomainNode;
		char			*fNISDomainName;
};

#endif
