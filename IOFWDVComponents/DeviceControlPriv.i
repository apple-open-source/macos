/*
	File:		DeviceControlPriv.i

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(KW)	Kevin Williams
		(GDW)	George D. Wilson Jr.

	Change History (most recent first):

		 <3>	 6/15/99	KW		fix screwup
		 <2>	 6/15/99	KW		add subtype
		 <1>	 6/15/99	GDW		first checked in
*/


#include <DeviceControl.i>



typedef UInt32 DeviceConnectionID;

enum
{
	kDeviceControlComponentType = 'devc',			/* Component type */
	kDeviceControlSubtypeFWDV = 'fwdv'				/* Component subtype */
};




/* Private calls made by the Isoc component */


%TellEmitter "components" "prefix DeviceControl";

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlEnableAVCTransactions(ComponentInstance instance) = ComponentCall(0x100);

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlDisableAVCTransactions(ComponentInstance instance) = ComponentCall(0x101);

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlSetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID connectionID) = ComponentCall(0x102);

pascal <exportset=IDHLib_10>
 ComponentResult DeviceControlGetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID *connectionID) = ComponentCall(0x103);

%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";
