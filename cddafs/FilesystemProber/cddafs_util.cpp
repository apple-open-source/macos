/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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

// cddafs_util.c created by CJS on Mon 10-Apr-2000


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// System Includes
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <mach/mach_init.h>
#include <servers/netname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <sys/errno.h>
#include <sys/paths.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/loadable_fs.h>

// Libkern includes
#include <libkern/OSByteOrder.h>

// CoreFoundation Includes
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

// IOKit Includes
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMedia.h>

// Project includes
#include "cddafs_util.h"
#include "mntopts.h"
#include "AppleCDDAFileSystemDefines.h"

	#include "CDDATrackName.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG							0
#define DEBUG_LEVEL						0

#ifndef DEBUG_ASSERT_COMPONENT_NAME_STRING
	#define DEBUG_ASSERT_COMPONENT_NAME_STRING "cddafs.util"
#endif

#include <AssertMacros.h>

#if (DEBUG_LEVEL > 3)
#define DebugLog(x)		printf x
#else
#define DebugLog(x)
#endif

static int
UtilityMain ( int argc, const char * argv[] );

static int
MountMain ( int argc, const char * argv[] );

static CFDataRef
GetTrackData ( const char * 				bsdDevNode,
			   const QTOCDataFormat10Ptr	TOCData );

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Globals
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct mntopt gMountOptions[] =
{
	MOPT_STDOPTS,
	{ NULL }
};

static char		gAppleCDDAName[MFSNAMELEN] = "cddafs";
static char		gFileSuffix[] = ".aiff";

#define			kMaxPrefixSize		3
#define			kASCIINumberZero	0x30
#define			kASCIISpace			0x20

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	main -	This our main entry point to this utility.  We get called by
//			autodiskmount.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
main ( int argc, const char * argv[] )
{
	
	int		result 	= -1;
	
	#if DEBUG
	int		index	= 0;
	
	for ( index = 0; index < argc; index++ )
	{
		printf ( "[%d] = %s\n", index, argv[index] );
	}
	#endif
	
	if ( strcmp ( argv[0], kUtilExecutableName ) == 0 )
	{
		
		result = UtilityMain ( argc, argv );
		
	}
	
	else
	{
		result = MountMain ( argc, argv );
	}
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark - Utility Code
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ		
//	UtilityMain -	Returns FSUR_IO_SUCCESS if everything works, else it
//					returns one of the FSUR_XXX errors in loadable_fs.h
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static int
UtilityMain ( int argc, const char * argv[] )
{
	
	char				rawDeviceName[MAXPATHLEN];
	char				blockDeviceName[MAXPATHLEN];
	const char *		actionPtr 						= NULL;
	const char *		mountPointPtr 					= NULL;
	int					result 							= FSUR_IO_SUCCESS;
	boolean_t			isLocked 						= 0;
	boolean_t			isEjectable 					= 0;
	int					mountFlags						= MNT_RDONLY;
	
	// Verify our arguments
	result = ParseUtilityArgs ( argc, argv, &actionPtr, &mountPointPtr, &isEjectable, &isLocked );
	require ( ( result == 0 ), Exit );
	
	// Build our device name (full path), should end up with something like:
	// -- "/dev/disk1" or "/dev/disk2" or "/dev/disk3"
	
	sprintf ( rawDeviceName, "/dev/r%s", argv[2] );
	sprintf ( blockDeviceName, "/dev/%s", argv[2] );
	
	// call the appropriate routine to handle the given action argument after becoming root
	result = seteuid ( 0 );
	require_action ( ( result == 0 ), Exit, result = FSUR_INVAL );
	
	result = setegid ( 0 );
	
	if ( result )
	{
		DebugLog ( ( "cddafs.util: ERROR: setegid: %s\n", strerror ( errno ) ) );
	}
	
	DebugLog ( ( "Entering the switch with action = %s\n", actionPtr ) );
	
    switch ( *actionPtr )
	{
		
		case FSUC_PROBE:
			result = Probe ( rawDeviceName );
			break;
		
		case FSUC_MOUNT:
		case FSUC_MOUNT_FORCE:
			result = Mount ( blockDeviceName, mountPointPtr, mountFlags );
			break;
		
		case FSUC_UNMOUNT:
			result = Unmount ( mountPointPtr );
			break;
		
        default:
			// should never get here since DoVerifyArgs should handle this situation
			DisplayUsage ( kUsageTypeUtility, argv );
			result = FSUR_INVAL;
			break;
		
	}
	
	
Exit:
	
	
	DebugLog ( ( "cddafs.util: EXIT: %d = ", result ) );
	switch ( result )
	{
		
		case FSUR_LOADERR:
			DebugLog ( ( "FSUR_LOADERR\n" ) );
			break;
			
		case FSUR_INVAL:
			DebugLog ( ( "FSUR_INVAL\n" ) );
			break;

		case FSUR_IO_SUCCESS:
			DebugLog ( ( "FSUR_IO_SUCCESS\n" ) );
			break;
			
		case FSUR_IO_FAIL:
			DebugLog ( ( "FSUR_IO_FAIL\n" ) );
			break;
			
		case FSUR_RECOGNIZED:
			DebugLog ( ( "FSUR_RECOGNIZED\n" ) );
			break;
			
		case FSUR_MOUNT_HIDDEN:
			DebugLog ( ( "FSUR_MOUNT_HIDDEN\n" ) );
			break;
			
		case FSUR_UNRECOGNIZED:
			DebugLog ( ( "FSUR_UNRECOGNIZED\n" ) );
			break;
			
		default:
			DebugLog ( ( "default\n" ) );
			break;
		
	}
	
	check ( result == FSUR_IO_SUCCESS );
	exit ( result );
	
	return result;	// ...and make main fit the ANSI spec.
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ParseUtilityArgs -	This routine will make sure the arguments passed
//						in to us are copacetic. Here is how this utility is used:
//
//	usage: cddafs.util actionArg deviceArg [mountPointArg] [flagsArg]
//	actionArg:
//		-p (Probe for mounting)
//		-P (Probe for initializing - not supported)
//		-m (Mount)
//		-r (Repair - not supported)
//		-u (Unmount)
//		-M (Force Mount)
//		-i (Initialize - not supported)
//		
//	deviceArg:
//		sd2 (for example)
//
//	mountPointArg:
//		/foo/bar/ (required for Mount and Force Mount actions)
//
//	flagsArg:
//		(these are ignored for CDROMs)
//
//	examples:
//		cddafs.util -p sd2 removable writable
//		cddafs.util -p sd2 removable readonly
//		cddafs.util -m sd2 /my/cddafs
//
//	Returns FSUR_INVAL if we find a bad argument, else 0.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
ParseUtilityArgs ( 	int				argc,
					const char *	argv[],
					const char **	actionPtr,
					const char **	mountPointPtr,
					boolean_t *		isEjectablePtr,
					boolean_t *		isLockedPtr )
{
	
	int			result 			= FSUR_INVAL;
	int			deviceLength	= 0;
	int			index			= 0;
	
	// Must have at least 3 arguments and the action argument must start with a '-'
	require_action ( ( argc >= 3 ), Exit, DisplayUsage ( kUsageTypeUtility, argv ) );
	require_action ( ( argv[1][0] == '-' ), Exit, DisplayUsage ( kUsageTypeUtility, argv ) );
	
	// we only support actions Probe, Mount, Force Mount, and Unmount
	*actionPtr = &argv[1][1];
	
	switch ( argv[1][1] )
	{
		
		case FSUC_PROBE:
			// action Probe and requires 5 arguments (need the flags)
			require_action ( ( argc >= 5 ), Exit, DisplayUsage ( kUsageTypeUtility, argv ) );
			index = 3;
			break;
		
		case FSUC_UNMOUNT:
			*mountPointPtr = argv[3];
			index = 0; // No isEjectable/isLocked flags for unmount.
			break;
			
		case FSUC_MOUNT:
		case FSUC_MOUNT_FORCE:
			// action Mount and ForceMount require 6 arguments
			// ( need the mountpoint and the flags )
			require_action ( ( argc >= 6 ), Exit, DisplayUsage ( kUsageTypeUtility, argv ) );
			
			*mountPointPtr = argv[3];
			index = 4;
			break;
			
		default:
			DisplayUsage ( kUsageTypeUtility, argv );
			goto Exit;
			break;
		
	}
	
	// Make sure device (argv[2]) is something reasonable
	// (we expect something like "disk1")
	deviceLength = strlen ( argv[2] );
	require ( ( deviceLength >= 5 ), Exit );
	
	result = 0;
	
	// If index is zero, no more work to do...
	require ( ( index != 0 ), Exit );
	
	// Flags: removable/fixed
	if ( !strcmp ( argv[index], "removable" ) )
	{
		
		*isEjectablePtr = 1;
		
	}
	
	else if ( !strcmp ( argv[index], "fixed" ) )
	{
		
		*isEjectablePtr = 0;
		
	}
	
	else
	{
		
		DebugLog ( ( "cddafs.util: ERROR: unrecognized flag (removable/fixed) argv[%d]='%s'\n",
					index, argv[index] ) );
		
	}
	
	// Flags: readonly/writable
	if ( !strcmp ( argv[index + 1], "readonly" ) )
	{
		*isLockedPtr = 1;
	}
	
	else if ( !strcmp ( argv[index + 1], "writable" ) )
	{
		*isLockedPtr = 0;
	}
	
	else
	{
		DebugLog ( ( "cddafs.util: ERROR: unrecognized flag (readonly/writable) argv[%d]='%s'\n",
					index, argv[index + 1] ) );
	}
	
	
Exit:
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Probe -		This routine will open the given raw device and check to
//				make sure there is media that looks like an Audio CD. Returns
//				FSUR_MOUNT_HIDDEN if everything works, else FSUR_IO_FAIL.
//	
//	deviceNamePtr - pointer to the raw device name (full path, like /dev/rdisk1)
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
Probe ( char * deviceNamePtr )
{

	int				result 	= FSUR_UNRECOGNIZED;
	UInt8 *			ptr 	= NULL;
	
	DebugLog ( ( "ENTER: Probe('%s')\n", deviceNamePtr ) );
	
	ptr = GetTOCDataPtr ( deviceNamePtr );
	if ( ptr != NULL )
	{
	
		// Parse the TOC for Audio Tracks
		result = ParseTOC ( ptr );
		
	}
	
	else
	{
		
		DebugLog ( ( "GetTOCDataPtr returned NULL.\n" ) );
		
	}
	
	// if we recognized the disc, create the name and suffix files
	if ( result == FSUR_RECOGNIZED )
	{
		
		CFStringRef			albumName 			= 0;
		CDDATrackName *		database 			= NULL;
		
		database = new CDDATrackName;
		
		if ( database != NULL )
		{
			
			DebugLog ( ( "database != NULL\n" ) );
			
			database->Init ( deviceNamePtr, ptr );
			
			DebugLog ( ( "Init called\n" ) );
			
			albumName = database->GetAlbumName ( );
			DebugLog ( ( "GetAlbumName called\n" ) );
			
		}
		
		if ( albumName != 0 )
		{
			
			Boolean		success				= false;
			char		buffer[MAXNAMLEN];
			
			#if DEBUG
			CFShow ( albumName );
			#endif
			
			success = _CFStringGetFileSystemRepresentation ( albumName,
														     ( UInt8 * ) buffer,
															 MAXNAMLEN );
			
			if ( success == true )
			{
				
				WriteDiskLabel ( buffer );
				
			}
			
			else
			{
				
				// Good old "Audio CD" should work...
				WriteDiskLabel ( kMountPointName );
				
			}
			
			// release it
			CFRelease ( albumName );
			albumName = 0;
			
		}
		
		else
		{
			
			// Good old "Audio CD" should work...
			WriteDiskLabel ( kMountPointName );
			
		}
		
		if ( database != NULL )
		{
			
			delete database;
			database = NULL;
			
		}
		
	}
	
	if ( ptr != NULL )
	{
		
		// free the memory
		free ( ptr );
		ptr = NULL;
		
	}
	
	DebugLog ( ( "Probe: returns " ) );
    
    switch ( result )
	{
		
		case FSUR_IO_FAIL:
			DebugLog ( ( "FSUR_IO_FAIL\n" ) );
			break;
			
		case FSUR_RECOGNIZED:
			DebugLog ( ( "FSUR_RECOGNIZED\n" ) );
			break;
			
		case FSUR_MOUNT_HIDDEN:
			DebugLog ( ( "FSUR_MOUNT_HIDDEN\n" ) );
			break;
		
		case FSUR_UNRECOGNIZED:
			DebugLog ( ( "FSUR_UNRECOGNIZED\n" ) );
			break;
        
		default:
			DebugLog ( ( "default\n" ) );
			break;
		
	}
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Unmount -	This routine will fire off a system command to unmount the
//				given device. Returns FSUR_IO_SUCCESS if everything works,
//				else FSUR_IO_FAIL.
//
//	theDeviceNamePtr - pointer to the device name (full path, like /dev/disk1s2).
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


int
Unmount ( const char * theMountPointPtr )
{
	
	int		result;
	int		mountflags = 0;
	
	result = unmount ( theMountPointPtr, mountflags );
	require_action ( ( result == 0 ), Exit, result = FSUR_IO_FAIL );
	
	result = FSUR_IO_SUCCESS;
	
	
Exit:
	
		
	return result;
	
}


#if 0
#pragma mark -
#pragma mark - Mount Code
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ		
//	MountMain -	returns 0 if successful
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static int
MountMain ( int argc, const char * argv[] )
{
	
	int		error 		= 0;
	int		mountFlags 	= 0;
	
	error = ParseMountArgs ( &argc, &argv, &mountFlags );
	require_action ( ( error == 0 ), Exit, DisplayUsage ( kUsageTypeMount, argv ) );
	
	error = Mount ( argv[0], argv[1], mountFlags );
	check ( error == FSUR_IO_SUCCESS );
	
	// mount_cddafs must return 0 for successful mounts
	if ( error == FSUR_IO_SUCCESS )
		error = 0;
	
	
Exit:
	
	
	return error;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ		
//	ParseMountArgs - Parses mount arguments
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
ParseMountArgs ( int * argc, const char ** argv[], int * mountFlags )
{
	
	int		error = 0;
	int		ch;
	
	*mountFlags = 0;
	
	// Audio CD's are read-only
	*mountFlags |= MNT_RDONLY;
	
	// Must have at least 3 arguments and the action argument must start with a '-'
	require_action ( ( *argc > 2 ), Exit, error = 1 );
	
	// Check command line args
	while ( ( ch = getopt ( *argc, ( char * const * ) *argv, "o:" ) ) != -1 )
	{
		
		switch ( ch )
		{
			
            case 'o':
				getmntopts ( optarg, gMountOptions, mountFlags, 0 );
				break;
			
            default:
				error = 1;
				break;
			
		}
		
	}
	
	*argc -= optind;
	*argv += optind;
	
	
Exit:
	
	
	return error;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	WriteDiskLabel -	This routine will create a file system info file that
//						is used by autodiskmount.  After creating the file it will
//						write whatever contentsPtr points to the new file.
//	
//	We end up with a file something like:
//			/usr/filesystems/cddafs.fs/cddafs.name
//
//	when our file system name is "cddafs" and suffixPtr points
//	to ".name" or ".label"
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
WriteDiskLabel ( char * contentsPtr )
{
	
	StripTrailingSpaces ( contentsPtr );
	write ( 1, contentsPtr, strlen ( contentsPtr ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	StripTrailingSpaces -	Strips trailing white spaces from character array
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
StripTrailingSpaces ( char * theContentsPtr )
{
	
	check ( theContentsPtr );
	check ( strlen ( theContentsPtr ) > 0 );
	
	if ( strlen ( theContentsPtr ) > 0 )
	{
		
		char    	*myPtr;
		
		myPtr = theContentsPtr + strlen ( theContentsPtr ) - 1;
		while ( *myPtr == kASCIISpace && myPtr >= theContentsPtr )
		{
			
			*myPtr = 0x00;
			myPtr--;
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Mount -	Attempts to mount on our filesystem if possible
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
Mount ( const char * 	deviceNamePtr,
		const char * 	mountPointPtr,
		int				mountFlags )
{
	
	AppleCDDAArguments 		args		= { 0 };
	struct vfsconf			vfc			= { 0 };
	int						result		= FSUR_IO_FAIL;
	int						error 		= 0;
	CFDataRef				data		= 0;
	CFDataRef				xmlDataRef	= 0;
	UInt32					size		= 0;
	QTOCDataFormat10Ptr		TOCDataPtr 	= NULL;
	UInt8 *					xmlDataPtr 	= NULL;
	
	DebugLog ( ( "Mount('%s','%s')\n", deviceNamePtr, mountPointPtr ) );
	
	require ( ( mountPointPtr != NULL ), Exit );
	require ( ( *mountPointPtr != '\0' ), Exit );
	
	args.device 		= ( char * ) deviceNamePtr;
	args.fileType		= ( UInt32 ) 'AIFC';
	args.fileCreator	= ( UInt32 ) '????';
	
	// Check if we're loaded into vfs or not.
	error = GetVFSConfigurationByName ( gAppleCDDAName, &vfc );
	if ( error != 0 )
	{
		
		// Kernel extension wasn't loaded, so try to load it...
		error = LoadKernelExtension ( );
		require ( ( error == 0 ), Exit );
		
		// Now try again since we loaded our extension
		error = GetVFSConfigurationByName ( gAppleCDDAName, &vfc );
		require ( ( error == 0 ), Exit );
		
	}
	
	TOCDataPtr = ( QTOCDataFormat10Ptr ) GetTOCDataPtr ( args.device );
	require ( ( TOCDataPtr != NULL ), Exit );
	
	data = GetTrackData ( args.device, TOCDataPtr );
	require ( ( data != NULL ), ReleaseTOCData );
	
	size = CFDataGetLength ( data );
	
	DebugLog ( ( "CFDataGetLength returned size = %ld\n", size ) );
		
	args.nameData 		= ( char * ) malloc ( size );
	args.nameDataSize 	= size;
	CFDataGetBytes ( data, CFRangeMake ( 0, size ), ( UInt8 * ) args.nameData );
	CFRelease ( data );
	data = 0;
	
	// Get the number of audio tracks
	args.numTracks = FindNumberOfAudioTracks ( TOCDataPtr );
	
	// build the XML file ".TOC.plist"
	xmlDataRef			= CreateXMLFileInPListFormat ( TOCDataPtr );
	xmlDataPtr			= ( UInt8 * ) CFDataGetBytePtr ( xmlDataRef );
	args.xmlFileSize	= CFDataGetLength ( xmlDataRef );
	args.xmlData 		= ( UInt8 * ) malloc ( args.xmlFileSize );
	
	require ( ( args.xmlData != NULL ), ReleaseNameData );
	
	// Copy the raw data from the CFData object to our mount args
	memcpy ( args.xmlData, xmlDataPtr, args.xmlFileSize );
	
	#if (DEBUG_LEVEL > 3)
	{
		UInt32	count = 0;
		
		for ( ; count < args.xmlFileSize; count = count + 8 )
		{
			
			DebugLog ( ("%x:%x:%x:%x %x:%x:%x:%x\n",
					xmlDataPtr[count],
					xmlDataPtr[count+1],
					xmlDataPtr[count+2],
					xmlDataPtr[count+3],
					xmlDataPtr[count+4],
					xmlDataPtr[count+5],
					xmlDataPtr[count+6],
					xmlDataPtr[count+7] ) );
			
		}
		
		DebugLog ( ( "\n" ) );
		DebugLog ( ( "XML File Size = %ld\n", args.xmlFileSize ) );
		
	}
	#endif
	
	
	// Print out the device name for debug purposes
	DebugLog ( ( "DeviceName = %s\n", args.device ) );
	DebugLog ( ( "numTracks = %d\n", args.numTracks ) );
	
	require ( ( args.nameData != NULL ), Exit );
	require ( ( args.nameDataSize != 0 ), Exit );
	
	// Issue the system mount command
	result = mount ( vfc.vfc_name, mountPointPtr, mountFlags, &args );
	require ( ( result == 0 ), ReleaseXMLData );
	
	result = FSUR_IO_SUCCESS;
	
	
ReleaseXMLData:

	
	require_quiet ( ( args.xmlData != NULL ), Exit );
	free ( ( char * ) args.xmlData );
	args.xmlData = NULL;
	
	
ReleaseNameData:
	
	
	require_quiet ( ( args.nameData != NULL ), Exit );
	free ( ( char * ) args.nameData );
	args.nameData = NULL;
	
	
ReleaseTOCData:
	
	
	// free the memory
	require_quiet ( ( TOCDataPtr != NULL ), Exit );
	free ( ( char * ) TOCDataPtr );
	TOCDataPtr = NULL;
	
	
Exit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ParseTOC - 	Parses the TOC to find audio tracks. If it finds one or more
//				audio tracks, it returns FSUR_RECOGNIZED,
//				else FSUR_UNRECOGNIZED
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
ParseTOC ( UInt8 * TOCInfoPtr )
{

	int						result				= FSUR_UNRECOGNIZED;
	int						error				= 0;
	QTOCDataFormat10Ptr		TOCDataPtr			= NULL;
	UInt8					index				= 0;
	UInt8					numberOfDescriptors = 0;
	
	require ( ( TOCInfoPtr != NULL ), Exit );
	
	// Set our pointer to the TOCInfoPtr
	TOCDataPtr = ( QTOCDataFormat10Ptr ) TOCInfoPtr;
	
	error = GetNumberOfTrackDescriptors ( TOCDataPtr, &numberOfDescriptors );
	require_quiet ( ( error == 0 ), Exit );
	
	for ( index = 0; index < numberOfDescriptors; index++ )
	{
		
		if ( IsAudioTrack ( index, TOCDataPtr ) )
		{
			
			// Found an audio cd
			return FSUR_RECOGNIZED;
			
		}
		
	}
	
	
Exit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// GetVFSConfigurationByName - 	Given a filesystem name, determine if it is
//								resident in the kernel, and if it is resident,
//								return its vfsconf structure.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
GetVFSConfigurationByName ( const char * fileSystemName, struct vfsconf * vfsConfPtr )
{

    int 		name[4];
    int			maxTypeNum;
    int			index;
    int			error;
	int			result;
	size_t 		buflen;
	
	name[0] = CTL_VFS;
	name[1] = VFS_GENERIC;
	name[2] = VFS_MAXTYPENUM;
	buflen 	= 4;
	
	error = sysctl ( name, 3, &maxTypeNum, &buflen, ( void * ) 0, ( size_t ) 0 );
	require_action ( ( error >= 0 ), Exit, result = -1 );
	
	name[2] = VFS_CONF;
	buflen = sizeof ( *vfsConfPtr );
	
	for ( index = 0; index < maxTypeNum; index++ )
	{
		
		name[3] = index;
		error = sysctl ( name, 4, vfsConfPtr, &buflen, ( void * ) 0, ( size_t ) 0 );
		if ( error < 0 )
		{
			
			require_action ( ( errno == EOPNOTSUPP ) || ( errno == ENOENT ), Exit, result = -1 );			
			continue;
			
		}
		
		// If the names are same, return no error
		if ( !strcmp ( fileSystemName, vfsConfPtr->vfc_name ) )
		{
			
			result = 0;
			goto Exit;
			
		}
		
	}
	
	errno	= ENOENT;
	result	= -1;
	
	
Exit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// GetTrackData - Loads databases and calls them for Track Name info.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFDataRef
GetTrackData ( const char * 				bsdDevNode,
			   const QTOCDataFormat10Ptr	TOCData )
{
	
	CFMutableDataRef	data			= 0;
	CFStringRef			trackString		= 0;
	UInt8				numTracks		= 0;
	UInt32				trackIndex		= 0;
	UInt8				currentTrack	= 0;
	SInt32				error			= 0;
	CDDATrackName *		database		= NULL;
	
	data 		= CFDataCreateMutable ( kCFAllocatorDefault, 0 );
	
	database	= new CDDATrackName;
	
	database->Init ( bsdDevNode, TOCData );
	
	error = GetNumberOfTrackDescriptors ( ( QTOCDataFormat10Ptr ) TOCData, &numTracks );
	if ( error != 0 )
	{
		
		DebugLog ( ( "Error = %ld on GetNumberOfTrackDescriptors\n", error ) );
		exit ( 1 );
		
	}
	
	for ( trackIndex = 0; trackIndex < numTracks; trackIndex++ )
	{
		
		if ( IsAudioTrack ( trackIndex, TOCData ) == false )
			continue;
		
		currentTrack = GetPointValue ( trackIndex, TOCData );
		trackString = ( database )->GetTrackName ( currentTrack );
		
		if ( trackString != 0 )
		{
			
			UInt8		size 				= 0;
			UInt8		suffixSize			= 0;
			UInt8		prefixSize			= 2;
			UInt8		bufferSize			= 0;
			UInt8		offset				= 0;
			Boolean		success				= false;
			char *		buffer				= NULL;
			char		prefix[kMaxPrefixSize];
			
			if ( currentTrack > 9 )
			{
				prefixSize++;
				prefix[offset++] = currentTrack / 10 + kASCIINumberZero;
			}
			
			prefix[offset++] = currentTrack % 10 + kASCIINumberZero;
			prefix[offset] = kASCIISpace;
			
			suffixSize = strlen ( gFileSuffix );
			bufferSize = MAXNAMLEN - prefixSize - suffixSize - 1;
			
			buffer = ( char * ) calloc ( 1, bufferSize );
			
			success = _CFStringGetFileSystemRepresentation ( trackString,
														     ( UInt8 * ) buffer,
															 bufferSize );
			
			buffer[bufferSize - 1] = 0;
			size = strlen ( buffer ) + prefixSize + suffixSize;
			
			DebugLog ( ( "size = %ld\n", size ) );
			DebugLog ( ( "buffer = %s", buffer ) );
			
			// Add the track number to the data object
			CFDataAppendBytes ( data,
								&currentTrack,
								1 );
			
			// Add the size to the data object
			CFDataAppendBytes ( data,
								&size,
								1 );
			
			// Add the prefix to the data object
			CFDataAppendBytes ( data,
								( UInt8 * ) prefix,
								prefixSize );
			
			// add the string to the data object
			CFDataAppendBytes ( data,
								( UInt8 * ) buffer,
								strlen ( buffer ) );
			
			// add the suffix to the data object
			CFDataAppendBytes ( data,
								( UInt8 * ) gFileSuffix,
								suffixSize );
			
			free ( buffer );
			
			// release it
			CFRelease ( trackString );
			
		}
		
	}
	
	if ( database != NULL )
	{
		
		delete database;
		database = NULL;
		
	}
	
	return data;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// LoadKernelExtension - 	Loads our filesystem kernel extension.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
LoadKernelExtension ( void )
{
	
	int 			pid;
	int 			result 	= -1;
	union wait 		status;
	
	pid = fork ( );
	
	if ( pid == 0 )
	{
		
		result = execl ( kLoadCommand, kLoadCommand,
						 kCDDAFileSystemExtensionPath, NULL );
		
		// We can only get here if the exec failed
		goto Exit;
		
	}
	
	if ( pid == -1 )
	{
		
		// fork() didn't work, so we grab the error from errno
		// to return as our error
		result = errno;
		goto Exit;
		
	}
	
	// Success!
	if ( ( wait4 ( pid, ( int * ) &status, 0, NULL ) == pid ) &&
		 ( WIFEXITED ( status ) ) )
	{
		
		// Stuff the status return code into our result
		result = status.w_retcode;
		
	}
	
	else
	{
		
		// else return -1
		result = -1;
		
	}
	
	
	check ( result == 0 );
	
	
Exit:
	
	
	return result;

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateXMLFileInPListFormat -	Makes a plist-style XML file which has a
//									parsed TOC. This makes it easy for
//									applications to get TOC info without having
//									to deal with the IOKit registry.
//	
//	TOCDataPtr - pointer to a QTOCDataFormat10 structure
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFDataRef
CreateXMLFileInPListFormat ( QTOCDataFormat10Ptr TOCDataPtr )
{
	
	SubQTOCInfoPtr			trackDescriptorPtr		= NULL;
	SubQTOCInfoPtr			lastTrackDescriptorPtr	= NULL;
	UInt16					numSessions				= 0;
	UInt16					length					= 0;
	UInt16					numberOfDescriptors 	= 0;
	CFMutableDictionaryRef	theCDDictionaryRef		= 0;
	CFMutableArrayRef		theSessionArrayRef		= 0;
	CFDataRef				theRawTOCDataRef		= 0;
	UInt32 					index 					= 0;
	CFDataRef				xmlData					= 0;
	
	DebugLog ( ( "CreateXMLFileInPListFormat called\n" ) );

	if ( TOCDataPtr != NULL )
	{
		
		// Create the master dictionary inside which all elements go
		theCDDictionaryRef = CFDictionaryCreateMutable ( 	kCFAllocatorDefault,
															0,
															&kCFTypeDictionaryKeyCallBacks,
															&kCFTypeDictionaryValueCallBacks );
		
		// Grab the length and advance
		length = OSSwapBigToHostInt16 ( TOCDataPtr->TOCDataLength );
		
		// Add the Raw TOC Data
		theRawTOCDataRef = CFDataCreate (	kCFAllocatorDefault,
											( UInt8 * ) TOCDataPtr,
											length + sizeof ( TOCDataPtr->TOCDataLength ) );
		
		CFDictionarySetValue ( 	theCDDictionaryRef,
								CFSTR ( kRawTOCDataString ),
								theRawTOCDataRef );
		
		length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
					sizeof ( TOCDataPtr->lastSessionNumber ) );
		
		numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );
		numSessions = TOCDataPtr->lastSessionNumber - TOCDataPtr->firstSessionNumber + 1;
		
		if ( numberOfDescriptors <= 0 )
		{
			
			DebugLog ( ( "No tracks on this CD...\n" ) );
			
		}
		
		// Create the array of sessions
		theSessionArrayRef 		= CFArrayCreateMutable ( kCFAllocatorDefault, numSessions, NULL );
		trackDescriptorPtr 		= TOCDataPtr->trackDescriptors;
		lastTrackDescriptorPtr	= TOCDataPtr->trackDescriptors + numberOfDescriptors - 1;
		
		for ( index = 0; trackDescriptorPtr <= lastTrackDescriptorPtr; index++ )
		{
			
			CFMutableDictionaryRef		theSessionDictionaryRef = 0;
			CFMutableArrayRef			theTrackArrayRef 		= 0;
			UInt32						trackIndex				= 0;
			
			theSessionDictionaryRef = CFDictionaryCreateMutable ( 	kCFAllocatorDefault,
																	0,
																	&kCFTypeDictionaryKeyCallBacks,
																	&kCFTypeDictionaryValueCallBacks );

			theTrackArrayRef = CFArrayCreateMutable ( 	kCFAllocatorDefault,
														0,
														&kCFTypeArrayCallBacks );

			while ( ( trackDescriptorPtr <= lastTrackDescriptorPtr ) &&
					( trackDescriptorPtr->sessionNumber == ( index + 1 ) ) )
			{
				
				CFMutableDictionaryRef	theTrackRef 	= 0;
				CFBooleanRef			isDigitalData	= kCFBooleanFalse;
				CFBooleanRef			preEmphasis		= kCFBooleanFalse;
				CFNumberRef				startBlock;
				CFNumberRef				sessionNumber;
				CFNumberRef				point;
				SInt16					pointValue;
				UInt32					blockAddress;
								
				pointValue = trackDescriptorPtr->point;
				
				if ( pointValue == 0x00A0 )
				{
					
					CFNumberRef		sessionType		= 0;
					CFNumberRef		firstTrackNum	= 0;
					CFNumberRef		sessionNumber	= 0;
					
					firstTrackNum = CFNumberCreate ( 	kCFAllocatorDefault,
														kCFNumberCharType,
														&trackDescriptorPtr->PMSF.A0PMSF.firstTrackNum );

					sessionType = CFNumberCreate ( 	kCFAllocatorDefault,
													kCFNumberCharType,
													&trackDescriptorPtr->PMSF.A0PMSF.discType );
					
					sessionNumber = CFNumberCreate ( 	kCFAllocatorDefault,
														kCFNumberCharType,
														&trackDescriptorPtr->sessionNumber );

					CFDictionarySetValue ( 	theSessionDictionaryRef,
											CFSTR ( kFirstTrackInSessionString ),
											firstTrackNum );

					CFDictionarySetValue ( 	theSessionDictionaryRef,
											CFSTR ( kSessionTypeString ),
											sessionType );
									
					CFDictionarySetValue ( 	theSessionDictionaryRef,
											CFSTR ( kSessionNumberString ),
											sessionNumber );
					
					CFRelease ( firstTrackNum );
					CFRelease ( sessionType );
					CFRelease ( sessionNumber );

					goto nextIteration;
					
				}
				
				if ( pointValue == 0x00A1 )
				{
					
					CFNumberRef		lastTrackNum	= 0;
					
					lastTrackNum = CFNumberCreate ( 	kCFAllocatorDefault,
														kCFNumberCharType,
														&trackDescriptorPtr->PMSF.A1PMSF.lastTrackNum );

					CFDictionarySetValue ( 	theSessionDictionaryRef,
											CFSTR ( kLastTrackInSessionString ),
											lastTrackNum );
					
					CFRelease ( lastTrackNum );
					
					goto nextIteration;
					
				}
				
				if ( pointValue == 0x00A2 )
				{
				
					CFNumberRef		leadoutBlock	= 0;
					UInt32			blockAddress	= 0;
					
					blockAddress = ( ( trackDescriptorPtr->PMSF.leadOutStartPosition.minutes * 60 ) +
									 trackDescriptorPtr->PMSF.leadOutStartPosition.seconds ) * 75 +
									 trackDescriptorPtr->PMSF.leadOutStartPosition.frames;
					
					leadoutBlock = CFNumberCreate ( 	kCFAllocatorDefault,
														kCFNumberIntType,
														&blockAddress );
				
					CFDictionarySetValue ( 	theSessionDictionaryRef,
											CFSTR ( kLeadoutBlockString ),
											leadoutBlock );
					
					CFRelease ( leadoutBlock );
				
					goto nextIteration;
				
				}
				
				if ( pointValue > 0x63 )
				{
					
					// Skip the B0-C1 identifiers
					goto nextIteration;
					
				}


				theTrackRef = CFDictionaryCreateMutable ( 	kCFAllocatorDefault,
															0,
															&kCFTypeDictionaryKeyCallBacks,
															&kCFTypeDictionaryValueCallBacks );

				point = CFNumberCreate ( 	kCFAllocatorDefault,
											kCFNumberSInt16Type,
											&pointValue );
				
				CFDictionarySetValue ( 	theTrackRef,
										CFSTR ( kPointString ),
										point );
				
				CFRelease ( point );
				
				
				blockAddress = ( (  trackDescriptorPtr->PMSF.startPosition.minutes * 60 ) +
									trackDescriptorPtr->PMSF.startPosition.seconds ) * 75 +
									trackDescriptorPtr->PMSF.startPosition.frames;
				
				DebugLog ( ( "track = %d, blockAddress = %ld\n", pointValue, blockAddress ) );
				
				startBlock = CFNumberCreate ( 	kCFAllocatorDefault,
												kCFNumberLongType,
												&blockAddress );
				
				CFDictionarySetValue ( 	theTrackRef,
										CFSTR ( kStartBlockString ),
										startBlock );
				
				CFRelease ( startBlock );
				
				sessionNumber = CFNumberCreate ( 	kCFAllocatorDefault,
													kCFNumberCharType,
													&trackDescriptorPtr->sessionNumber );
				
				CFDictionarySetValue ( 	theTrackRef,
										CFSTR ( kSessionNumberString ),
										sessionNumber );
				
				CFRelease ( sessionNumber );
				
				if ( ( trackDescriptorPtr->control & kDigitalDataMask ) == kDigitalDataMask )
				{
					
					isDigitalData = kCFBooleanTrue;
				
				}
				
				CFDictionarySetValue ( 	theTrackRef,
										CFSTR ( kDataString ),
										isDigitalData );
				
				if ( ( trackDescriptorPtr->control & kPreEmphasisMask ) == kPreEmphasisMask )
				{
					
					preEmphasis = kCFBooleanTrue;
					
				}
				
				CFDictionarySetValue ( theTrackRef,
									   CFSTR ( kPreEmphasisString ),
									   preEmphasis );
				
				// Add the dictionary to the array
				CFArraySetValueAtIndex ( theTrackArrayRef, trackIndex, theTrackRef );

				CFRelease ( theTrackRef );
				trackIndex++;
				
				
nextIteration:
				
				
				// Advance to next track
				trackDescriptorPtr++;
								
			}
			
			// Set the array inside of the dictionary for the session
			CFDictionarySetValue ( theSessionDictionaryRef, CFSTR ( kTrackArrayString ), theTrackArrayRef );
			CFArraySetValueAtIndex ( theSessionArrayRef, index, theSessionDictionaryRef );
			
		}
		
		CFDictionarySetValue ( theCDDictionaryRef, CFSTR ( kSessionsString ), theSessionArrayRef );
		
		CFRelease ( theSessionArrayRef );
		
		xmlData = CFPropertyListCreateXMLData ( kCFAllocatorDefault, theCDDictionaryRef );
		
		CFRelease ( theCDDictionaryRef );
		
	}
	
	return xmlData;
	
}


#if 0
#pragma mark -
#pragma mark - Shared Code
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DisplayUsage -	This routine will do a printf of the correct usage
//					for whichever utility was launched.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
DisplayUsage ( int usageType, const char * argv[] )
{
	
	if ( usageType == kUsageTypeMount )
	{
		
		printf ( "usage: mount_cddafs [-o options] device-name mount-point\n" );
		
	}
	
	else if ( usageType == kUsageTypeUtility )
	{
		
		printf ( "usage: %s action_arg device_arg [mount_point_arg] [Flags] \n", argv[0] );
		printf ( "action_arg:\n" );
		printf ( "       -%c (Probe for mounting)\n", FSUC_PROBE );
		printf ( "       -%c (Mount)\n", FSUC_MOUNT );
		printf ( "       -%c (Unmount)\n", FSUC_UNMOUNT );
		printf ( "       -%c (Force Mount)\n", FSUC_MOUNT_FORCE );
		printf ( "device_arg:\n" );
		printf ( "       device we are acting upon (for example, 'sd2')\n" );
		printf ( "mount_point_arg:\n" );
		printf ( "       required for Mount and Force Mount \n" );
		printf ( "Flags:\n" );
		printf ( "       required for Mount, Force Mount and Probe\n" );
		printf ( "       indicates removable or fixed (for example 'fixed')\n" );
		printf ( "       indicates readonly or writable (for example 'readonly')\n" );
		printf ( "Examples:\n");
		printf ( "       %s -p sd2 fixed writable\n", argv[0] );
		printf ( "       %s -m sd2 /my/hfs removable readonly\n", argv[0] );
		
	}
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTOCDataPtr -	Gets a pointer to the TOCData
//	
//	deviceNamePtr - pointer to the device name (full path, like /dev/rdisk1)
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8 *
GetTOCDataPtr ( const char * deviceNamePtr )
{

	UInt8 *					ptr 			= NULL;
	kern_return_t			error			= 0;
	mach_port_t				masterPort		= 0;
	io_iterator_t			iterator		= 0;
	io_registry_entry_t		registryEntry	= 0;
	CFMutableDictionaryRef	properties 		= 0;
	CFDataRef     			data			= 0;
	char *					bsdName 		= NULL;
		
	if ( !strncmp ( deviceNamePtr, "/dev/r", 6 ) )
	{
		
		// Strip off the /dev/ from /dev/rdiskX
		bsdName = ( char * ) &deviceNamePtr[6];
		
	}

	else if ( !strncmp ( deviceNamePtr, "/dev/", 5 ) )
	{
		
		// Strip off the /dev/ from /dev/diskX
		bsdName = ( char * ) &deviceNamePtr[5];
		
	}
	
	else
	{
		
		DebugLog ( ( "GetTOCDataPtr: ERROR: not /dev/something...\n" ) );
		goto Exit;
		
	}
	
	error = IOMasterPort ( bootstrap_port, &masterPort );
	require ( ( error == KERN_SUCCESS ), Exit );
	
	error = IOServiceGetMatchingServices ( 	masterPort,
											IOBSDNameMatching ( masterPort, 0, bsdName ),
											&iterator );
	require ( ( error == KERN_SUCCESS ), Exit );
	
	// Only expect one entry since there is a 1:1 correspondence between bsd names
	// and IOKit storage objects	
	registryEntry = IOIteratorNext ( iterator );
	require ( ( registryEntry != NULL ), ReleaseIterator );
	
	require ( IOObjectConformsTo ( registryEntry, kIOCDMediaString ), ReleaseEntry );
	
	error = IORegistryEntryCreateCFProperties ( registryEntry,
												&properties,
												kCFAllocatorDefault,
												kNilOptions );
	require ( ( error == KERN_SUCCESS ), ReleaseEntry );
	
	// Get the TOCInfo
	data = ( CFDataRef ) CFDictionaryGetValue ( properties, 
												CFSTR ( kIOCDMediaTOCKey ) );
	if ( data != NULL )
	{
		
		ptr = CreateBufferFromCFData ( data );
		
	}
	
	// Release the properties
	CFRelease ( properties );
	
	
ReleaseEntry:
	
	
	// release the object
	error = IOObjectRelease ( registryEntry );
	
	
ReleaseIterator:
	
	
	// relese the iterator
	error = IOObjectRelease ( iterator );
	
	
Exit:
	
	
	return ptr;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// IsAudioTrack - Figures out if a track is audio or not.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean
IsAudioTrack ( UInt32 trackNumber, QTOCDataFormat10Ptr TOCData )
{
	
	SubQTOCInfoPtr	trackDescriptorPtr;
	
	trackDescriptorPtr = TOCData->trackDescriptors;
	trackDescriptorPtr = trackDescriptorPtr + trackNumber;
	
	if ( trackDescriptorPtr->point < 100 && trackDescriptorPtr->point > 0 )
	{
						
		if ( ( trackDescriptorPtr->control & kDigitalDataMask ) == 0 )
		{
			
			// Found an audio track
			return true;
			
		}
		
	}
	
	return false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// GetNumberOfTrackDescriptors - Gets the number of track descriptors
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SInt32
GetNumberOfTrackDescriptors ( 	QTOCDataFormat10Ptr	TOCDataPtr,
								UInt8 * 			numberOfDescriptors )
{
	
	UInt16	length	= 0;
	SInt32	result	= 0;
	
	require_action ( ( TOCDataPtr != NULL ), Exit, result = -1 );
	require_action ( ( numberOfDescriptors != NULL ), Exit, result = -1 );
	
	// Grab the length and advance
	length = OSSwapBigToHostInt16 ( TOCDataPtr->TOCDataLength );
	DebugLog ( ( "Length = %d\n", length ) );
	
	require_action ( ( length > sizeof ( CDTOC ) ), Exit, result = -1 );
	
	length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
				sizeof ( TOCDataPtr->lastSessionNumber ) );
	
	*numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );
	DebugLog ( ( "Number of descriptors = %d\n", *numberOfDescriptors ) );
	
	
Exit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// GetPointValue - Gets the track's point value
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
GetPointValue ( UInt32 trackIndex, QTOCDataFormat10Ptr TOCData )
{
	
	SubQTOCInfoPtr	trackDescriptorPtr;
	
	trackDescriptorPtr = TOCData->trackDescriptors;
	trackDescriptorPtr = trackDescriptorPtr + trackIndex;
	
	return trackDescriptorPtr->point;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateBufferFromCFData - Allocates memory for a chunk of memory and copies
//							 the contents of the CFData to it.
//
//	NB:	The calling function should dispose of the memory
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8 *
CreateBufferFromCFData ( CFDataRef theData )
{

	CFRange				range;
	CFIndex          	bufferLength 	= 0;
	UInt8 *           	buffer			= NULL;
	
	bufferLength	= CFDataGetLength ( theData );
	buffer			= ( UInt8 * ) malloc ( bufferLength );
	
	range = CFRangeMake ( 0, bufferLength );
	
	if ( buffer != NULL )
		CFDataGetBytes ( theData, range, buffer );
	
	return buffer;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindNumberOfAudioTracks - 	Parses the TOC to find the number of audio
//								tracks.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
FindNumberOfAudioTracks ( QTOCDataFormat10Ptr TOCDataPtr )
{

	UInt32					result				= 0;
	SubQTOCInfoPtr			trackDescriptorPtr	= NULL;
	UInt16					length				= 0;
	UInt16					numberOfDescriptors = 0;
	
	DebugLog ( ( "FindNumberOfAudioTracks called\n" ) );
	
	require ( ( TOCDataPtr != NULL ), Exit );
	
	// Grab the length and advance
	length = OSSwapBigToHostInt16 ( TOCDataPtr->TOCDataLength );
	require ( ( length > sizeof ( CDTOC ) ), Exit );
	
	length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
				sizeof ( TOCDataPtr->lastSessionNumber ) );
	
	numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );
	require ( ( numberOfDescriptors != 0 ), Exit );
	
	DebugLog ( ( "numberOfDescriptors = %d\n", numberOfDescriptors ) );	
	
	trackDescriptorPtr = TOCDataPtr->trackDescriptors;
	
	while ( numberOfDescriptors > 0 )
	{
		
		if ( trackDescriptorPtr->point < 100 && trackDescriptorPtr->point > 0 )
		{
			
			if ( ( trackDescriptorPtr->control & kDigitalDataMask ) == 0 )
			{
				
				// Found an audio track
				result++;
				
			}
			
		}
		
		trackDescriptorPtr++;
		numberOfDescriptors--;
		
	}
	
	
Exit:
	
	
	DebugLog ( ( "numberOfTracks = %ld\n", result ) );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
