/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: MacRISC4_PlatformPlugin.cpp,v $
//		Revision 1.2  2003/02/18 00:02:07  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#include "MacRISC4_PlatformPlugin.h"

#define super IOPlatformPlugin
OSDefineMetaClassAndStructors(MacRISC4_PlatformPlugin, IOPlatformPlugin)

/*
 * start
 */
bool MacRISC4_PlatformPlugin::start(IOService *nub)
{

	return super::start (nub);
}
