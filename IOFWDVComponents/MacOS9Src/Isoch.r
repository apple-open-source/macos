/*
	File:		Isoc.r

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Written by:	Richard Sepulveda

	Copyright:	й 1998-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(jkl)	Jay Lloyd
		(SW)	Sean Williams
		(RS)	Richard Sepulveda

	Change History (most recent first):

		 <5>	 8/17/99	jkl		Added a little better ID's.
		 <4>	 6/28/99	RS		Added unregister flag to component flags.
	   <OX3>	 6/11/99	SW		MasterInterfaces integration
		 <2>	 5/19/99	RS		Changed resource name from 'DVIS' to 'dvis' to be more
									consistent with Apple resource names.
	To Do:
*/

#define thng_RezTemplateVersion 1

#include "Types.r"
#include "Components.r"

resource 'thng' (-20771, "DV_IHandler", locked) { //еее
	'ihlr',
	'dv  ',
	'appl',
	0x80000000,
	kAnyComponentFlagsMask,
	'dvis',
	-20771,	//еее
	'STR ',
	0,
	'STR ',
	0,
	'ICON',
	0,

	// extended thng resource info follows
	(0 << 16) |												// component version
	(0),
	componentHasMultiplePlatforms  |						// Registration Flags
		componentDoAutoVersion | componentWantsUnregister,	// More flags | componentWantsUnregister
	0,														// icon family id
	{
		cmpWantsRegisterMessage,							// Register Flags
		'dvis',												// Resource Type
		-20771,	//еее										// component code id
		platformPowerPC										// platform type
	}
};
