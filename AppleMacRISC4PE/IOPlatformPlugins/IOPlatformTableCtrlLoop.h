/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformTableCtrlLoop.h,v $
//		Revision 1.4  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.3  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.2.3  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.2.2.2  2003/05/23 06:36:57  eem
//		More registration notification stuff.
//		
//		Revision 1.2.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.2  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.1.2.1  2003/05/16 07:08:46  eem
//		Table-lookup active fan control working with this checkin.
//		
//
//

#ifndef _IOPLATFORMTABLECTRLLOOP_H
#define _IOPLATFORMTABLECTRLLOOP_H

#include "IOPlatformCtrlLoop.h"

// This key holds our lookup table array in the metastate description
#define kIOPTCLLookupTableKey	"LookupTable"

class IOPlatformStateSensor;
class IOPlatformControl;

class IOPlatformTableCtrlLoop : public IOPlatformCtrlLoop
{

	OSDeclareDefaultStructors(IOPlatformTableCtrlLoop)

protected:

	IOPlatformStateSensor * inputSensor;
	IOPlatformControl * outputControl;

	OSArray * lookupTable;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

public:

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);

	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );

	virtual bool updateMetaState( void );
	virtual void adjustControls( void );

};

#endif //_IOPLATFORMTABLECTRLLOOP_H
