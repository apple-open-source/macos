/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <mach/mach_init.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>

#include "GetRegistry.h"
#include "FSParticular.h"

#include "DiskArbitrationServerMain.h"

/*
-----------------------------------------------------------------------------------------
*/

typedef enum
{
    kDriveTypeUnknown = 0x00,
    kDriveTypeHD      = 0x01,
    kDriveTypeCD      = 0x02,
    kDriveTypeMO      = 0x03,
    kDriveTypeDVD     = 0x04
} DriveType;

DriveType GetDriveType(io_registry_entry_t media);

/*
-----------------------------------------------------------------------------------------
*/



void GetDisksFromRegistry(io_iterator_t iter)
{
	kern_return_t		kr;
	io_registry_entry_t	entry;

	io_name_t		ioMediaName;
	UInt32			ioBSDUnit;
	int				ioWhole, ioWritable, ioEjectable, ioLeaf;
	DriveType		driveType;
	unsigned		flags;
        mach_port_t 		masterPort;
        mach_timespec_t		timeSpec;

        timeSpec.tv_sec = 5;
        timeSpec.tv_nsec = 5;
        
        IOMasterPort(bootstrap_port, &masterPort);

        //sleep(1);
        IOKitWaitQuiet(masterPort , &timeSpec);

	while ( entry = IOIteratorNext( iter ) )
	{
		char *          ioBSDName  = NULL; // (needs release)
		char *          ioContent  = NULL; // (needs release)

		CFBooleanRef    boolean    = 0; // (don't release)
		CFNumberRef     number     = 0; // (don't release)
		CFDictionaryRef properties = 0; // (needs release)
		CFStringRef     string     = 0; // (don't release)

                //CFDictionaryRef ioMatchingDictionary = NULL;

		io_string_t	ioDeviceTreePath;
		char *		ioDeviceTreePathPtr;

		// MediaName

		kr = IORegistryEntryGetName(entry, ioMediaName);
		if ( KERN_SUCCESS != kr )
		{
			dwarning(("can't obtain name for media object\n"));
			goto Next;
		}

		// Get Properties

		kr = IORegistryEntryCreateCFProperties(entry, &properties, kCFAllocatorDefault, kNilOptions);
		if ( KERN_SUCCESS != kr )
		{
			dwarning(("can't obtain properties for '%s'\n", ioMediaName));
			goto Next;
		}

		assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

		// BSDName
		
		string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDNameKey));
		if ( ! string )
		{
			/* We're only interested in disks accessible via BSD */
			dwarning(("kIOBSDNameKey property missing for '%s'\n", ioMediaName));
			goto Next;
		}

		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioBSDName = daCreateCStringFromCFString(string);
		assert(ioBSDName);

		dwarning(("ioBSDName = '%s'\t", ioBSDName));

		// BSDUnit
		
		number = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDUnitKey));
		if ( ! number )
		{
			/* We're only interested in disks accessible via BSD */
			dwarning(("\nkIOBSDUnitKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(number) == CFNumberGetTypeID());

		if ( ! CFNumberGetValue(number, kCFNumberSInt32Type, &ioBSDUnit) )
		{
			goto Next;
		}

		dwarning(("ioBSDUnit = %ld\t", ioBSDUnit));

		// Content

		string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaContentKey));
		if ( ! string )
		{
			dwarning(("\nkIOMediaContentKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioContent = daCreateCStringFromCFString(string);
		assert(ioContent);

		dwarning(("ioContent = '%s'\t", ioContent));
	
		// Leaf

		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaLeafKey));
		if ( ! boolean )
		{
			dwarning(("\nkIOMediaLeafKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioLeaf = ( kCFBooleanTrue == boolean );

		dwarning(("ioLeaf = %d\t", ioLeaf));
	
		// Whole
		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWholeKey));
		if ( ! boolean )
		{
			dwarning(("\nkIOMediaWholeKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWhole = ( kCFBooleanTrue == boolean );

		dwarning(("ioWhole = %d\t", ioWhole));

		// Writable
	
		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWritableKey));
		if ( ! boolean )
		{
			dwarning(("\nkIOMediaWritableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWritable = ( kCFBooleanTrue == boolean );

		dwarning(("ioWritable = %d\t", ioWritable));
	
		// Ejectable

		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaEjectableKey));
		if ( ! boolean )
		{
			dwarning(("\nkIOMediaEjectableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioEjectable = ( kCFBooleanTrue == boolean );

		dwarning(("ioEjectable = %d\t", ioEjectable));

		/* Obtain the device tree path. */

		kr = IORegistryEntryGetPath( entry, kIODeviceTreePlane, ioDeviceTreePath );
		if ( kr )
		{
                    // this warning is unneeded, since many volumes won't have this pointer.
			// dwarning(( "\nERROR: IORegistryEntryGetPath -> %d\n", kr ));
			ioDeviceTreePathPtr = NULL;
		}
		else
		{
			dwarning(( "ioDeviceTreePath = '%s'\t", ioDeviceTreePath ));
			if ( strlen( ioDeviceTreePath ) < strlen( "IODeviceTree:" ) )
			{
				dwarning(( "\nERROR: expected leading 'IODeviceTree:' in ioDeviceTreePath\n"));
				ioDeviceTreePathPtr = ioDeviceTreePath; /* for lack of a better alternative */
			}
			else
			{
				ioDeviceTreePathPtr = ioDeviceTreePath + strlen( "IODeviceTree:" );
				dwarning(( "\ntrimmed ioDeviceTreePath = '%s'\n", ioDeviceTreePathPtr ));
			}
		}
		
		// Construct the <flags> word
		
		flags = 0;

		if ( ! ioWritable ) 
			flags |= kDiskArbDiskAppearedLockedMask;

		if ( ioEjectable ) 
			flags |= kDiskArbDiskAppearedEjectableMask;

		if ( ioWhole ) 
			flags |= kDiskArbDiskAppearedWholeDiskMask;
		
		if ( ! ioLeaf ) 
			flags |= kDiskArbDiskAppearedNonLeafDiskMask;

		// DriveType
		
		driveType = GetDriveType( entry );
		switch ( driveType )
		{
			case kDriveTypeHD:
				/* do nothing */
			break;
			case kDriveTypeCD:
				flags |= kDiskArbDiskAppearedCDROMMask;
			break;
			case kDriveTypeMO:
				/* do nothing */
			break;
			case kDriveTypeDVD:
				flags |= kDiskArbDiskAppearedDVDROMMask;
			break;
			case kDriveTypeUnknown:
				/* do nothing */
			break;
			default:
				/* do nothing */
			break;
		}
		
		// Create a disk record

                if (!shouldAutomount(entry)) {
                        flags |= kDiskArbDiskAppearedNoMountMask;
                }

                {

                        DiskPtr dp;

                        /* Is there an existing disk on our list with this IOBSDName? */

                        dp = LookupDiskByIOBSDName( ioBSDName );
                        if ( dp )
                        {
                                dwarning(("%s: '%s' already exists\n", __FUNCTION__, ioBSDName));

                                /* In case it was accidentally unmounted, mark it for remounting */
                                if ( dp->mountpoint && 0==strcmp(dp->mountpoint,"") )
                                {
                                        dp->state = kDiskStateNew;
                                }
                        }
                        else
                        {
                                /* Create a new disk, leaving the <mountpoint> initialized to NULL */

                                if ( ! NewDisk(	ioBSDName,
                                                                ioBSDUnit,
                                                                ioContent,
                                                                kDiskFamily_SCSI,
                                                                NULL,
                                                                ioMediaName,
                                                                ioDeviceTreePathPtr,
                                                                entry,
                                                                flags ) )
                                {
                                        LogErrorMessage("%s: NewDisk() failed!\n", __FUNCTION__);
                                }
                        }
                }
		
	Next:
	
		if ( properties )	CFRelease( properties );
		if ( ioBSDName )	free( ioBSDName );
		if ( ioContent )	free( ioContent );

		// IOObjectRelease( entry );
		
	} /* while */

} /* GetDisksFromRegistry */


/*
-----------------------------------------------------------------------------------------
*/


DriveType GetDriveType(io_registry_entry_t media)
{
    io_registry_entry_t parent  = 0; // (needs release)
    io_iterator_t       parents = 0; // (needs release)
    io_registry_entry_t service = media; // mandatory initialization
    DriveType           type    = kDriveTypeUnknown; // mandatory initialization
    kern_return_t       kr;

    while ( service )
    {
        kr = IORegistryEntryGetParentIterator( service, kIOServicePlane, & parents );
        if ( kr != KERN_SUCCESS ) break;

        if ( parent ) IOObjectRelease( parent );

        parent = IOIteratorNext( parents );
        if ( parent == 0 ) break;

        if ( IOObjectConformsTo( parent, "IOBlockStorageDriver" ) ) break;
        //if ( IOObjectConformsTo( parent, "IODrive" ) ) break;

        IOObjectRelease( parents );
		parents = 0;

        service = parent;
    }

    if ( parent )
    {
    if ( IOObjectConformsTo( parent, "IODVDBlockStorageDriver" ) )
    //if ( IOObjectConformsTo( parent, "IODVDDrive" ) )
		{
			dwarning(("DriveType = DVD\n"));
            type = kDriveTypeDVD;
		}
    else if ( IOObjectConformsTo( parent, "IOMOBlockStorageDriver" ) )
    //else if ( IOObjectConformsTo( parent, "IOMODrive" ) )
		{
			dwarning(("DriveType = MO\n"));
            type = kDriveTypeMO;
		}
    else if ( IOObjectConformsTo( parent, "IOCDBlockStorageDriver" ) )
    //else if ( IOObjectConformsTo( parent, "IOCDDrive" ) )
		{
			dwarning(("DriveType = CD\n"));
            type = kDriveTypeCD;
		}
        else
		{
			dwarning(("DriveType = HD\n"));
            type = kDriveTypeHD;
		}
        
        IOObjectRelease( parent );
    }

    if ( parents ) IOObjectRelease( parents );

    return type;
}

int shouldAutomount(io_registry_entry_t media)
{
    io_registry_entry_t parent  = 0; // (needs release)
    io_iterator_t       parents = 0; // (needs release)
    io_registry_entry_t service = media; // mandatory initialization
    kern_return_t       kr;

    int			mount = 1;  // by default mount

    while ( service )
    {
        kr = IORegistryEntryGetParentIterator( service, kIOServicePlane, & parents );
        if ( kr != KERN_SUCCESS ) break;

        if ( parent ) IOObjectRelease( parent );

        parent = IOIteratorNext( parents );
        if ( parent == 0 ) break;

        if ( IOObjectConformsTo( parent, "IOHDIXHDDriveNub" ) )
        {
            // Get Properties
            CFDictionaryRef properties = 0; // (needs release)
            CFBooleanRef    boolean    = 0; // (don't release)

            kr = IORegistryEntryCreateCFProperties(parent, &properties, kCFAllocatorDefault, kNilOptions);

            assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

            boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR("autodiskmount"));

            assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

            if (!( kCFBooleanTrue == boolean )) {
                mount = 0;
            }

            break;

        }

        IOObjectRelease( parents );
        parents = 0;

        service = parent;
    }

    if ( parent ) IOObjectRelease( parent );
    if ( parents ) IOObjectRelease( parents );

    return mount;
}


/*
-----------------------------------------------------------------------------------------
*/

