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
#include <architecture/byte_order.h>
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

// CoreFoundation Includes
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

// IOKit Includes
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDMedia.h>

// Project includes
#include "cddafs_util.h"
#include "mntopts.h"
#include "AppleCDDAFileSystemDefines.h"

	#include "CDDATrackName.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DISK_ARB_COMPATIBILITY_MODE		0
#define	DEBUG							0

#if DEBUG
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
	if ( result != 0 )
	{
		goto Exit;
	}
	
	// Build our device name (full path), should end up with something like:
	// -- "/dev/disk1" or "/dev/disk2" or "/dev/disk3"
	
	sprintf ( rawDeviceName, "/dev/r%s", argv[2] );
	sprintf ( blockDeviceName, "/dev/%s", argv[2] );


	// call the appropriate routine to handle the given action argument after becoming root
	result = seteuid ( 0 );
	
	if ( result )
	{
		
		DebugLog ( ( "cddafs.util: ERROR: seteuid(0): %s\n", strerror ( errno ) ) );
		result = FSUR_INVAL;
		goto Exit;
		
	}
	
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
	if ( ( argc < 3 ) || ( argv[1][0] != '-' ) )
	{
		
		DisplayUsage ( kUsageTypeUtility, argv );
		goto Exit;
		
	}
	
	// we only support actions Probe, Mount, Force Mount, and Unmount
	
	*actionPtr = &argv[1][1];
	
	switch ( argv[1][1] )
	{
		
		case FSUC_PROBE:
			// action Probe and requires 5 arguments (need the flags)
			if ( argc < 5 )
			{
				
				DisplayUsage ( kUsageTypeUtility, argv );
				goto Exit;
				
			}
			
			else
			{
				index = 3;
			}
			break;
		
		case FSUC_UNMOUNT:
			*mountPointPtr = argv[3];
			index = 0; // No isEjectable/isLocked flags for unmount.
			break;
			
		case FSUC_MOUNT:
		case FSUC_MOUNT_FORCE:
			// action Mount and ForceMount require 6 arguments
			// ( need the mountpoint and the flags )
			if ( argc < 6 )
			{
				
				DisplayUsage ( kUsageTypeUtility, argv );
				goto Exit;
				
			}
			
			else
			{
				*mountPointPtr = argv[3];
				index = 4;
			}
			break;
			
		default:
			DisplayUsage ( kUsageTypeUtility, argv );
			goto Exit;
			break;
		
	}
	
	// Make sure device (argv[2]) is something reasonable
	// (we expect something like "disk1")
	deviceLength = strlen ( argv[2] );
	if ( deviceLength < 5 )
	{
		
		DisplayUsage ( kUsageTypeUtility, argv );
		goto Exit;
		
	}

	if ( index )
	{
		
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
		
	}
	
	result = 0;
	
	
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
	
	if ( result != 0 )
	{
	
		result = FSUR_IO_FAIL;
		DebugLog ( ( "cddafs.util: ERROR: Unmount('%s') returned %d\n", theMountPointPtr, result ) );
	
	}
	
	else
	{
	
		result = FSUR_IO_SUCCESS;
	
	}
	
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
	if ( error != 0 )
	{
		
		DisplayUsage ( kUsageTypeMount, argv );
		goto Exit;
		
	}
	
	error = Mount ( argv[0], argv[1], mountFlags );
	
	
Exit:
	
	
	return error;
	
}


int
ParseMountArgs ( int * argc, const char ** argv[], int * mountFlags )
{
	
	int		error = 0;
	int		ch;
	
	*mountFlags = 0;
	
	// Audio CD's are read-only
	*mountFlags |= MNT_RDONLY;
	
	// Must have at least 3 arguments and the action argument must start with a '-'
	if ( *argc < 3 )
	{
		
		error = -1;
		goto Exit;
		
	}

	
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

#if DISK_ARB_COMPATIBILITY_MODE
	
	int			fileToWrite;

#endif /* DISK_ARB_COMPATIBILITY_MODE */
		
	StripTrailingSpaces ( contentsPtr );
	write ( 1, contentsPtr, strlen ( contentsPtr ) );
	
#if DISK_ARB_COMPATIBILITY_MODE
	
	for ( fileToWrite = 0; fileToWrite < 2; fileToWrite++ )
	{
		
		char   		fileName[MAXPATHLEN];
		int			fd;
		
		sprintf ( &fileName[0], "%s/%s%s/%s", FS_DIR_LOCATION, kCDDAFileSystemName,
				FS_DIR_SUFFIX, kCDDAFileSystemName );
		
		if ( fileToWrite == 0 )
			strcat ( &fileName[0], FS_NAME_SUFFIX );
		else
			strcat ( &fileName[0], FS_LABEL_SUFFIX );
		
		unlink ( &fileName[0] );	// erase existing file
		
		int oldMask = umask ( 0 );
				
		fd = open ( &fileName[0], O_CREAT | O_TRUNC | O_WRONLY, 0644 );
		umask ( oldMask );
		
		if ( fd > 0 )
		{
		
			write ( fd, contentsPtr, strlen ( contentsPtr ) );
			close ( fd );
		
		}
		
		else
		{
		
			perror ( fileName );
			
		}
		
	}

#endif /* DISK_ARB_COMPATIBILITY_MODE */
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	StripTrailingSpaces -	Strips trailing white spaces from character array
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
StripTrailingSpaces ( char * theContentsPtr )
{
	
	if ( strlen ( theContentsPtr ) )
	{
		
		char    	*myPtr;
		
		myPtr = theContentsPtr + strlen( theContentsPtr ) - 1;
		while ( *myPtr == ' ' && myPtr >= theContentsPtr )
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
	
	AppleCDDAArguments 		args;
	struct vfsconf			vfc;
	int						result;
	int						error 		= 0;
	QTOCDataFormat10Ptr		TOCDataPtr 	= NULL;
	UInt8 *					xmlDataPtr 	= NULL;
	CFDataRef				data		= 0;
	CFDataRef				xmlDataRef	= 0;
	
	DebugLog ( ( "Mount('%s','%s')\n", deviceNamePtr, mountPointPtr ) );
	
	if ( mountPointPtr == NULL || *mountPointPtr == '\0' )
	{
		
		result = FSUR_IO_FAIL;
		return result;
		
	}
		
	( void ) memset ( &args, 0L, sizeof ( AppleCDDAArguments ) );
	
	args.device 		= ( char * ) deviceNamePtr;
	args.fileType		= ( UInt32 ) 'AIFC';
	args.fileCreator	= ( UInt32 ) '????';
	
	// Check if we're loaded into vfs or not.
	error = GetVFSConfigurationByName ( gAppleCDDAName, &vfc );
	
	if ( error )
	{
		
		// Kernel extension wasn't loaded, so try to load it...
		error = LoadKernelExtension ( );
		
		// Error while loading...barf
		if ( error != 0 )
		{
			return error;
		}
		
		else
		{
			
			// Now try again since we loaded our extension
			error = GetVFSConfigurationByName ( gAppleCDDAName, &vfc );

			if ( error != 0 )
			{
				return error;
			}
		
		}
		
	}
	
	TOCDataPtr = ( QTOCDataFormat10Ptr ) GetTOCDataPtr ( args.device );
	if ( TOCDataPtr != NULL )
	{
		
		data = GetTrackData ( args.device, TOCDataPtr );
		if ( data != 0 )
		{
			
			UInt32	size = CFDataGetLength ( data );
			
			DebugLog ( ( "CFDataGetLength returned size = %ld\n", size ) );
			
			args.nameData 		= ( char * ) malloc ( size );
			args.nameDataSize 	= size;
			CFDataGetBytes ( data, CFRangeMake ( 0, size ), ( UInt8 * ) args.nameData );
			CFRelease ( data );
			data = 0;
			
		}
		
		else
		{
			DebugLog ( ( "data is null\n" ) );
		}
		
		// Get the number of audio tracks
		args.numTracks = FindNumberOfAudioTracks ( TOCDataPtr );
		
		// build the XML file ".TOC.plist"
		xmlDataRef			= CreateXMLFileInPListFormat ( TOCDataPtr );
		xmlDataPtr			= ( UInt8 * ) CFDataGetBytePtr ( xmlDataRef );
		args.xmlFileSize	= CFDataGetLength ( xmlDataRef );
		args.xmlData 		= ( UInt8 * ) malloc ( args.xmlFileSize );
		
		if ( args.xmlData != NULL )
		{
			
			// Copy the raw data from the CFData object to our mount args
			memcpy ( args.xmlData, xmlDataPtr, args.xmlFileSize );
			
			#if 0
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
			
		}
		
		// free the memory
		free ( ( char * ) TOCDataPtr );
		
	}
	
	// Print out the device name for debug purposes
	DebugLog ( ( "DeviceName = %s\n", args.device ) );
	DebugLog ( ( "numTracks = %d\n", args.numTracks ) );
	
	if ( args.nameData == NULL || args.nameDataSize == 0 )
		return FSUR_IO_FAIL;
	
	// Issue the system mount command
	result = mount ( vfc.vfc_name, mountPointPtr, mountFlags, &args );
	
	if ( args.xmlData != NULL )
	{
		
		// free the memory
		free ( ( char * ) xmlDataPtr );
		
	}	
	
	if ( args.nameData != NULL )
	{
		
		// free the memory
		free ( ( char * ) args.nameData );
		
	}
	
	if ( result == 0 )
		result = FSUR_IO_SUCCESS;
	
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
		
	if ( TOCInfoPtr != NULL )
	{
		
		// Set our pointer to the TOCInfoPtr
		TOCDataPtr = ( QTOCDataFormat10Ptr ) TOCInfoPtr;
		
		error = GetNumberOfTrackDescriptors ( TOCDataPtr, &numberOfDescriptors );
		if ( error == 0 )
		{
		
			for ( index = 0; index < numberOfDescriptors; index++ )
			{
			
				if ( IsAudioTrack ( index, TOCDataPtr ) )
				{
											
					// Found an audio cd
					return FSUR_RECOGNIZED;
					
				}
				
			}
		
		}
		
	}
		
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

    int 		name[4], maxtypenum, cnt, error;
	size_t 		buflen;
	
	name[0] = CTL_VFS;
	name[1] = VFS_GENERIC;
	name[2] = VFS_MAXTYPENUM;
	buflen 	= 4;
	
	error = sysctl ( name, 3, &maxtypenum, &buflen, ( void * ) 0, ( size_t ) 0 );
	if ( error < 0 )
		return ( -1 );
	
	name[2] = VFS_CONF;
	buflen = sizeof ( *vfsConfPtr );
	
	for ( cnt = 0; cnt < maxtypenum; cnt++ )
	{
		
		name[3] = cnt;
		error = sysctl ( name, 4, vfsConfPtr, &buflen, ( void * ) 0, ( size_t ) 0 );
		if ( error < 0 )
		{
			
			if ( errno != EOPNOTSUPP && errno != ENOENT )
			{
				
				return ( -1 );
				
			}
			
			continue;
			
		}
		
		// If the names are same, return no error
		if ( !strcmp ( fileSystemName, vfsConfPtr->vfc_name ) )
		{
			return ( 0 );
		}
		
	}
	
	errno = ENOENT;
	
	return ( -1 );
	
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
			
			UInt32		size 				= 0;
			UInt8		tmp					= 0;
			Boolean		success				= false;
			char		buffer[MAXNAMLEN];
			
			success = _CFStringGetFileSystemRepresentation ( trackString,
														     ( UInt8 * ) buffer,
															 MAXNAMLEN );
			
			size = strlen ( buffer );
			
			DebugLog ( ( "size = %ld\n", size ) );
			
			// add the track number to the data object
			CFDataAppendBytes ( data,
								&currentTrack,
								1 );
			
			// Size better be less than MAXNAMLEN!
			tmp = size & 0x000000FF;
			
			// add the size to the data object
			CFDataAppendBytes ( data,
								&tmp,
								1 );
			
			// add the string to the data object
			CFDataAppendBytes ( data,
								( UInt8 * ) buffer,
								tmp );
			
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
		goto Return;
		
	}
	
	if ( pid == -1 )
	{
		
		// fork() didn't work, so we grab the error from errno
		// to return as our error
		result = errno;
		goto Return;
		
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
	
	
Return:
	
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
		length = NXSwapBigShortToHost ( TOCDataPtr->TOCDataLength );
		
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
	if ( error != KERN_SUCCESS )
	{
		
		DebugLog ( ( "GetTOCDataPtr: ERROR: couldn't create master port...\n" ) );
		goto Exit;
		
	}
	
	error = IOServiceGetMatchingServices ( 	masterPort,
											IOBSDNameMatching ( masterPort, 0, bsdName ),
											&iterator );
	
	if ( error != KERN_SUCCESS )
	{
		
		DebugLog ( ( "GetTOCDataPtr: ERROR: no matching services...\n" ) );
		goto Exit;
		
	}
	
	// Only expect one entry since there is a 1:1 correspondence between bsd names
	// and IOKit storage objects	
	registryEntry = IOIteratorNext ( iterator );
	if ( registryEntry != NULL )
	{
		
		if ( IOObjectConformsTo ( registryEntry, kIOCDMediaString ) )
		{
			
			error = IORegistryEntryCreateCFProperties ( registryEntry, &properties,
														kCFAllocatorDefault, kNilOptions );
			if ( error == KERN_SUCCESS )
			{
				
				CFDataRef     data = 0;
				
				// Get the TOCInfo
				data = ( CFDataRef ) CFDictionaryGetValue ( properties, 
															CFSTR ( kIOCDMediaTOCKey ) );
				if ( data != NULL )
				{
					
					ptr = CreateBufferFromCFData ( data );

				}
				
				// Release the properties
				CFRelease ( properties );
				
			}
			
		}
		
		else
		{
			
			DebugLog ( ( "Dynamic Cast failed.\n" ) );
			goto Exit;
			
		}
		
		// release the object
		error = IOObjectRelease ( registryEntry );
		
	}
	
	else
	{
		
		DebugLog ( ( "Registry entry is NULL.\n" ) );
		
	}
	
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
	
	if ( ( TOCDataPtr == NULL )  || ( numberOfDescriptors == NULL ) )
	{
		return -1;
	}
	
	// Grab the length and advance
	length = NXSwapBigShortToHost ( TOCDataPtr->TOCDataLength );
	DebugLog ( ( "Length = %d\n", length ) );
	
	if ( length <= 4 )
		return -1;
	
	length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
				sizeof ( TOCDataPtr->lastSessionNumber ) );
	
	*numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );
	DebugLog ( ( "Number of descriptors = %d\n", *numberOfDescriptors ) );
	
	return 0;
	
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
	CFIndex          	bufferLength 	= CFDataGetLength ( theData );
	UInt8 *           	buffer			= ( UInt8 * ) malloc ( bufferLength );
	
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

	if ( TOCDataPtr != NULL )
	{
				
		// Grab the length and advance
		length = NXSwapBigShortToHost ( TOCDataPtr->TOCDataLength );
		length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
					sizeof ( TOCDataPtr->lastSessionNumber ) );
		
		numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );

		DebugLog ( ( "numberOfDescriptors = %d\n", numberOfDescriptors ) );
		
		if ( numberOfDescriptors <= 0 )
		{

			return result;

		}
			
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
		
	}

	DebugLog ( ( "numberOfTracks = %ld\n", result ) );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
