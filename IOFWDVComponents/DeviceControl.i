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


#include <MacTypes.i>
#include <Components.i>


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

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlDoAVCTransaction(ComponentInstance instance, DVCTransactionParams* params) = ComponentCall(1);


%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";
