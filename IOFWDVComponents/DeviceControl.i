/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
	File:		DeviceControl.i

	Contains:	Component API for doing AVC transactions.

	Version:	xxx put version here xxx

	Copyright:	© 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Jed (George) Wilson

		Other Contact:		Sean Williams

		Technology:			xxx put technology here xxx

	Writers:

		(KW)	Kevin Williams
		(GDW)	George D. Wilson Jr.

	Change History (most recent first):

		 <3>	 6/16/99	GDW		Changed AVC struct name to DVC for people that include
									"DVFamily.h".
		 <2>	 6/15/99	KW		Change name of Handler
		 <1>	 6/15/99	GDW		first checked in
		 <2>	 6/15/99	GDW		Created
*/


%if FRAMEWORKS	
	#include <CoreServices.i>
%else
    #include <MacTypes.i>
    #include <Components.i>
%endif


typedef extern UInt32	(*DCResponseHandler) (
	UInt32						fwCommandObjectID,
	Ptr							responseBuffer,
	UInt32						responseLength);


struct DVCTransactionParams {
	Ptr						commandBufferPtr;
	UInt32					commandLength;
	Ptr						responseBufferPtr;
	UInt32					responseBufferSize;
	DCResponseHandler *responseHandler;
};

%TellEmitter "components" "prefix DeviceControl";

pascal <exportset=IDHLib_10, exportset=fw_DVComponentGlue_X>
ComponentResult DeviceControlDoAVCTransaction(ComponentInstance instance, DVCTransactionParams* params) = ComponentCall(1);


%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";
