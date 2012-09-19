/*
 * Copyright (c) 1998-2012 Apple Inc. All rights reserved.
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

#include "DAPrivate.h"

#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAMount.h"
#include "DAQueue.h"
#include "DAStage.h"

#include <sysexits.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/wait.h>

static int __DAFileSystemSetAdoption( DAFileSystemRef filesystem, CFURLRef mountpoint, Boolean adoption )
{
    char * path   = NULL;
    int    status = 0;

    path = ___CFURLCopyFileSystemRepresentation( mountpoint );
    if ( path == NULL )  { status = EINVAL; goto __DAFileSystemSetAdoptionErr; }

    status = fork( );
    if ( status == -1 )  { status = errno; goto __DAFileSystemSetAdoptionErr; }

    if ( status == 0 )
    {
        execle( "/usr/sbin/vsdbutil",
                "/usr/sbin/vsdbutil",
                adoption ? "-a" : "-d",
                path,
                NULL,
                NULL );

        exit( EX_OSERR );
    }

    waitpid( status, &status, 0 );

    status = WIFEXITED( status ) ? ( ( char ) WEXITSTATUS( status ) ) : status;
    if ( status )  { goto __DAFileSystemSetAdoptionErr; }

__DAFileSystemSetAdoptionErr:

    if ( path )  free( path );

    return status;
}

static int __DAFileSystemSetEncoding( DAFileSystemRef filesystem, CFURLRef mountpoint, CFStringEncoding encoding )
{
    struct statfs fs     = { 0 };
    char *        path   = NULL;
    int           status = 0;

    path = ___CFURLCopyFileSystemRepresentation( mountpoint );
    if ( path == NULL )  { status = EINVAL; goto __DAFileSystemSetEncodingErr; }

    status = ___statfs( path, &fs, MNT_NOWAIT );
    if ( status == -1 )  { status = errno; goto __DAFileSystemSetEncodingErr; }

    status = fork( );
    if ( status == -1 )  { status = errno; goto __DAFileSystemSetEncodingErr; }

    if ( status == 0 )
    {
        char option[16];

        snprintf( option, sizeof( option ), "-o-e=%d", ( int ) encoding );

        execle( "/sbin/mount",
                "/sbin/mount",
                "-t",
                fs.f_fstypename,
                "-u",
                option,
                ( fs.f_flags & MNT_NODEV            ) ? "-onodev"    : "-odev",
                ( fs.f_flags & MNT_NOEXEC           ) ? "-onoexec"   : "-oexec",
                ( fs.f_flags & MNT_NOSUID           ) ? "-onosuid"   : "-osuid",
                ( fs.f_flags & MNT_RDONLY           ) ? "-ordonly"   : "-orw",
                ( fs.f_flags & MNT_IGNORE_OWNERSHIP ) ? "-onoowners" : "-oowners",
                fs.f_mntfromname,
                fs.f_mntonname,
                NULL,
                NULL );

        exit( EX_OSERR );
    }

    waitpid( status, &status, 0 );

    status = WIFEXITED( status ) ? ( ( char ) WEXITSTATUS( status ) ) : status;
    if ( status )  { goto __DAFileSystemSetEncodingErr; }

__DAFileSystemSetEncodingErr:

    if ( path )  free( path );

    return status;
}

DAReturn _DADiskRefresh( DADiskRef disk )
{
    DAReturn status;

    status = kDAReturnUnsupported;

    if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
    {
        struct statfs * mountList;
        int             mountListCount;
        int             mountListIndex;

        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            if ( strcmp( _DAVolumeGetID( mountList + mountListIndex ), DADiskGetID( disk ) ) == 0 )
            {
                break;
            }
        }

        if ( mountListIndex == mountListCount )
        {
            CFURLRef mountpoint;

            mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

            DAMountRemoveMountPoint( mountpoint );

            DADiskSetBypath( disk, NULL );

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaPathKey ) )
            {
                DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, NULL );

                DADiskDescriptionChangedCallback( disk, kDADiskDescriptionVolumePathKey );
            }
            else
            {
                DALogDebugHeader( "bsd [0] -> %s", gDAProcessNameID );

                DALogDebug( "  removed disk, id = %@.", disk );

                DADiskDisappearedCallback( disk );

                DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, NULL );

                DADiskSetState( disk, kDADiskStateZombie, TRUE );

                ___CFArrayRemoveValue( gDADiskList, disk );
            }

            DAStageSignal( );
        }

        status = kDAReturnSuccess;
    }
    else
    {
        struct statfs * mountList;
        int             mountListCount;
        int             mountListIndex;

        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            if ( strcmp( _DAVolumeGetID( mountList + mountListIndex ), DADiskGetID( disk ) ) == 0 )
            {
                break;
            }
        }

        if ( mountListIndex < mountListCount )
        {
            CFURLRef path;

            path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault,
                                                            ( void * ) mountList[mountListIndex].f_mntonname,
                                                            strlen( mountList[mountListIndex].f_mntonname ),
                                                            TRUE );

            if ( path )
            {
                DADiskSetBypath( disk, path );

                DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, path );

                DADiskDescriptionChangedCallback( disk, kDADiskDescriptionVolumePathKey );

                DAStageSignal( );

                CFRelease( path );
            }
        }

        status = kDAReturnSuccess;
    }

    return status;
}

DAReturn _DADiskSetAdoption( DADiskRef disk, Boolean adoption )
{
    CFURLRef path;
    DAReturn status;

    path = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );
    if ( path == NULL )  { status = kDAReturnBadArgument; goto _DADiskSetAdoptionErr; }

    status = __DAFileSystemSetAdoption( DADiskGetFileSystem( disk ), path, adoption );
    if ( status )  { status = unix_err( status ); goto _DADiskSetAdoptionErr; }

_DADiskSetAdoptionErr:

    return status;
}

DAReturn _DADiskSetEncoding( DADiskRef disk, CFStringEncoding encoding )
{
    CFMutableArrayRef keys   = NULL;
    CFStringRef       name1  = NULL;
    CFStringRef       name2  = NULL;
    CFURLRef          path1  = NULL;
    CFURLRef          path2  = NULL;
    DAReturn          status = kDAReturnSuccess;

    path1 = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );
    if ( path1 == NULL )  { status = kDAReturnBadArgument; goto _DADiskSetEncodingErr; }

    status = __DAFileSystemSetEncoding( DADiskGetFileSystem( disk ), path1, encoding );
    if ( status )  { status = unix_err( status ); goto _DADiskSetEncodingErr; }

    keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
    if ( keys == NULL )  { status = kDAReturnNoResources; goto _DADiskSetEncodingErr; }

    name1 = DADiskGetDescription( disk, kDADiskDescriptionVolumeNameKey );
    if ( name1 == NULL )  { status = kDAReturnError; goto _DADiskSetEncodingErr; }

    name2 = _DAFileSystemCopyName( DADiskGetFileSystem( disk ), path1 );
    if ( name2 == NULL )  { status = kDAReturnError; goto _DADiskSetEncodingErr; }

    status = CFEqual( name1, name2 );
///w:start
//  if ( status )  { status = kDAReturnSuccess; goto _DADiskSetEncodingErr; }
///w:stop

    DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name2 );

    CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );

///w:start
    if ( status == FALSE )
///w:stop
    path2 = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMove );
///w:start
    status = kDAReturnSuccess;
///w:stop

    if ( path2 )
    {
        DADiskSetBypath( disk, path2 );

        DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, path2 );

        CFArrayAppendValue( keys, kDADiskDescriptionVolumePathKey );
    }

    DADiskDescriptionChangedCallback( disk, keys );

_DADiskSetEncodingErr:

    if ( keys  )  CFRelease( keys  );
    if ( name2 )  CFRelease( name2 );
    if ( path2 )  CFRelease( path2 );

    return status;
}

Boolean _DAUnitIsUnreadable( DADiskRef disk )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef item;

        item = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( DADiskGetBSDUnit( disk ) == DADiskGetBSDUnit( item ) )
        {
            CFStringRef name;

            name = DADiskGetDescription( item, kDADiskDescriptionMediaBSDNameKey );

            if ( DADiskGetBusy( item ) )
            {
                return FALSE;
            }

            if ( DADiskGetClaim( item ) )
            {
                return FALSE;
            }

            if ( DADiskGetOption( item, kDADiskOptionMountAutomatic ) == FALSE )
            {
                return FALSE;
            }

            if ( DADiskGetDescription( item, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
            {
                return FALSE;
            }

            if ( DADiskGetDescription( item, kDADiskDescriptionMediaLeafKey ) == kCFBooleanFalse )
            {
                CFIndex subcount;
                CFIndex subindex;

                subcount = CFArrayGetCount( gDADiskList );

                for ( subindex = 0; subindex < subcount; subindex++ )
                {
                    DADiskRef subitem;

                    subitem = ( void * ) CFArrayGetValueAtIndex( gDADiskList, subindex );

                    if ( item != subitem )
                    {
                        CFStringRef subname;

                        subname = DADiskGetDescription( subitem, kDADiskDescriptionMediaBSDNameKey );

                        if ( subname )
                        {
                            if ( CFStringHasPrefix( subname, name ) )
                            {
                                break;
                            }
                        }
                    }
                }

                if ( subindex == subcount )
                {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}
