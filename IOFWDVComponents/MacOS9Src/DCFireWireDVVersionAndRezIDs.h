/*
	File:		DCFireWireDVVersionAndRezIDs.h

	Contains:	Contants that define the version of the Crush Isochronous Data Hanlder
				and any PRIVATE ResourceIDs it might need.

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Sean Williams

	Writers:


	Change History (most recent first):

	   <1>	 6/15/99		KW		Created
*/


#ifndef __DCFIREWIREDVVERSIONANDREZIDS__
#define __DCFIREWIREDVVERSIONANDREZIDS__



//
// IDH Crush Version Constants
//	When a component is queried with with the kComponentVersionSelect selector, it is
//	supposed to return the 'interface version' in the high word of the ComponentResult
//	and the 'code revision' in the low word of the Component result.

enum { kDCFireWireDVInterfaceVersion = 1 };


//
// kDCFireWireDVCodeVersion
// 1.0.0 - Initial release

#define	kMajorRevisionNumber		0x01
#define	kMinorRevisionNumber		0x00
#define kDevelopmentStage			development
#define kBuildVersionNumber			0x00
#define	kShortVersionString			"1.0.0d0"
#define	kLongVersionString			"1.0.0d0 © Apple Computer, Inc., 1999"


#define kDCFireWireDVCodeVersion (kMajorRevisionNumber << 8 | kMinorRevisionNumber)



#endif // __DCFIREWIREDVVERSIONANDREZIDS__ //
