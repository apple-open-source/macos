/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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

#include "DAMount.h"
#include "DAQueue.h"

#include "DABase.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DASupport.h"
#include "DATelemetry.h"

#include <mntopts.h>
#include <fstab.h>
#include <sys/stat.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <os/variant_private.h>

struct __DAMountCallbackContext
{
///w:start
    Boolean         automatic;
///w:stop
    IOPMAssertionID assertionID;
    DAMountCallback callback;
    void *          callbackContext;
    DADiskRef       disk;
    Boolean         force;
    CFURLRef        mountpoint;
    CFStringRef     options;
    CFURLRef        devicePath;
    DADiskRef       contDisk;
    int             fd;
    uint64_t        fsckStartTime;
    uint64_t        mountStartTime;
    Boolean         useUserFS;
};

typedef struct __DAMountCallbackContext __DAMountCallbackContext;

static void __DAMountWithArgumentsCallbackStage1( int status, void * context );
static void __DAMountWithArgumentsCallbackStage2( int status, void * context );
static void __DAMountWithArgumentsCallbackStage3( int status, void * context );

static void __DAMountWithArgumentsCallback( int status, void * parameter )
{
    /*
     * Process the mount request completion.
     */

    __DAMountCallbackContext * context = parameter;

///w:start
    if ( context->automatic )
    {
        if ( status == ___EDIRTY )
        {
            DAMountWithArguments( context->disk, NULL, context->callback, context->callbackContext, kDAFileSystemMountArgumentForce, kDAFileSystemMountArgumentNoWrite, NULL );

            context->callback = NULL;
        }
    }
///w:stop
    if ( context->callback )
    {
        ( context->callback )( status, context->mountpoint, context->callbackContext );
    }

    CFRelease( context->disk    );
    CFRelease( context->options );

    if ( context->mountpoint )  CFRelease( context->mountpoint );

    free( context );
}

static void __DAMountSendFSCKEvent( int status , __DAMountCallbackContext * context )
{
    DATelemetrySendFSCKEvent( status ,
                              context->disk ,
                              clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - context->fsckStartTime );
}

static void __DAMountWithArgumentsCallbackStage1( int status, void * parameter )
{
    /*
     * Process the repair command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( context->assertionID != kIOPMNullAssertionID )
    {
        IOPMAssertionRelease( context->assertionID );
        context->assertionID = kIOPMNullAssertionID;
    }
    if ( DADiskGetDescription( context->disk , kDADiskDescriptionRepairRunningKey ) != NULL )
    {
        DADiskSetDescription( context->disk , kDADiskDescriptionRepairRunningKey , NULL );
        DADiskDescriptionChangedCallback( context->disk , kDADiskDescriptionRepairRunningKey );
    }
#if TARGET_OS_IOS
    if ( context->contDisk )
    {
        DAUnitSetState( context->contDisk, kDAUnitStateCommandActive, FALSE );
        CFRelease( context->contDisk );
        context->contDisk = NULL;
    }
    if ( context->fd != -1)
    {
        close( context->fd );
    }
#endif
    if ( status )
    {
        /*
         * We were unable to repair the volume.
         */

        if ( status == ECANCELED )
        {
            status = 0;
        }
        else
        {
            DALogInfo( "repaired disk, id = %@, failure.", context->disk );
            DALogError( "unable to repair %@ (status code 0x%08X).", context->disk, status );
            __DAMountSendFSCKEvent( status , context );
            
            if ( context->force )
            {
                status = 0;
            }
            else
            {
                __DAMountWithArgumentsCallback( ___EDIRTY, context );
            }
        }
    }
    else
    {
        /*
         * We were able to repair the volume.
         */

        DADiskSetState( context->disk, kDADiskStateRequireRepair, FALSE );

        DALogInfo( "repaired disk, id = %@, success.", context->disk );
        __DAMountSendFSCKEvent( status , context );
    }

    /*
     * Mount the volume.
     */

    if ( status == 0 )
    {
        /*
         * Create the mount point, in case one needs to be created.
         */

#if TARGET_OS_OSX
        if ( context->mountpoint == NULL )
        {
            context->mountpoint = DAMountCreateMountPointWithAction( context->disk, kDAMountPointActionMake );
        }
#endif

        /*
         * Execute the mount command.
         */
#if TARGET_OS_IOS
        if ( context->mountpoint || DAMountGetPreference( context->disk, kDAMountPreferenceEnableUserFSMount ) == true)
#else
        if ( context->mountpoint )
#endif
        {
            DALogInfo( "mounted disk, id = %@, ongoing.", context->disk );
            DADiskSetState( context->disk, kDADiskStateMountOngoing , TRUE );
            
            if ( context->mountpoint )
            {
                CFArrayAppendValue( gDAMountPointList, context->mountpoint );
            }
            CFStringRef preferredMountMethod = NULL;
#if TARGET_OS_OSX
            preferredMountMethod = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountMethodkey );
#else
            if ( true == DAMountGetPreference( context->disk, kDAMountPreferenceEnableUserFSMount ) )
            {
                preferredMountMethod = CFSTR("UserFS");
            }
#endif
            context->useUserFS = DAFilesystemShouldMountWithUserFS( DADiskGetFileSystem( context->disk ) ,
                                                                    preferredMountMethod );
            context->mountStartTime = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
            DAFileSystemMountWithArguments( DADiskGetFileSystem( context->disk ),
                                            context->devicePath,
                                            DADiskGetDescription( context->disk, kDADiskDescriptionVolumeNameKey ),
                                            context->mountpoint,
                                            DADiskGetUserUID( context->disk ),
                                            DADiskGetUserGID( context->disk ),
                                            preferredMountMethod,
                                            __DAMountWithArgumentsCallbackStage2,
                                            context,
                                            context->options,
                                            NULL );
        }
        else
        {
            __DAMountWithArgumentsCallback( ENOSPC, context );
        }
    }
}

static void __DAMountWithArgumentsCallbackStage2( int status, void * parameter )
{
    /*
     * Process the mount command's completion.
     */
    
    __DAMountCallbackContext * context = parameter;
    DAFileSystemRef filesystem = DADiskGetFileSystem( context->disk );
    DADiskSetState( context->disk , kDADiskStateMountOngoing , FALSE );
    CFStringRef kind = NULL;
    Boolean automount = DADiskGetState( context->disk , _kDADiskStateMountAutomatic );
    Boolean isExternal = DADiskIsExternalVolume( context->disk );
    DATelemetryFSImplementation mountType = DATelemetryFSImplementationKext;
    
    if ( filesystem != NULL )
    {
        kind = DAGetFSTypeWithUUID( filesystem ,
                                    DADiskGetDescription( context->disk, kDADiskDescriptionVolumeUUIDKey ) );
        
        if ( context->useUserFS )
        {
#if TARGET_OS_OSX || TARGET_OS_IOS
            if ( __DAMountShouldUseFSKit( DAFileSystemGetKind( filesystem ) , NULL ) )
            {
                mountType = DATelemetryFSImplementationFSKit;
                
                if ( status == 0 )
                {
                    DADiskSetState( context->disk , _kDADiskStateMountedWithFSKit , TRUE );
                }
            }
            else
#endif
            {
                mountType = DATelemetryFSImplementationUserFS;
                
                if ( status == 0 )
                {
                    DADiskSetState( context->disk , _kDADiskStateMountedWithUserFS , TRUE );
                }
            }
        }
    }
    
    DATelemetrySendMountEvent( status ,
                               kind ,
                               mountType ,
                               automount ,
                               isExternal ,
                               clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - context->mountStartTime );
    
    if ( context->mountpoint )
    {
        ___CFArrayRemoveValue( gDAMountPointList, context->mountpoint );
    }

    if ( status )
    {
        /*
         * We were unable to mount the volume.
         */

        DALogInfo( "mounted disk, id = %@, failure.", context->disk );

        DALogError( "unable to mount %@ (status code 0x%08X).", context->disk, status );

        if ( context->mountpoint )
        {
            DAMountRemoveMountPoint( context->mountpoint );
        }

        __DAMountWithArgumentsCallback( status, context );
    }
    else
    {
        /*
         * We were able to mount the volume.
         */

        DALogInfo( "mounted disk, id = %@, success.", context->disk );

        if ( DADiskGetDescription( context->disk, kDADiskDescriptionMediaEncryptedKey ) == kCFBooleanTrue &&
             ( DAMountGetPreference( context->disk, kDAMountPreferenceDefer ) ) )
        {
            // set console user id
            DALogInfo( "setting uid, id = %@ %d, success.", context->disk, gDAConsoleUserUID  );
            DADiskSetMountedByUserUID( context->disk, gDAConsoleUserUID );
            
        }
        /*
         * Execute the "repair quotas" command.
         */

        if ( DADiskGetState( context->disk, kDADiskStateRequireRepairQuotas ) )
        {
          
            DAFileSystemRepairQuotas( DADiskGetFileSystem( context->disk ),
                                      context->mountpoint,
                                      __DAMountWithArgumentsCallbackStage3,
                                      context );
        }
        else
        {
            __DAMountWithArgumentsCallbackStage3( 0, context );
        }
    }
}

static void __DAMountWithArgumentsCallbackStage3( int status, void * parameter )
{
    /*
     * Process the "repair quotas" command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( status )
    {
        DALogError( "unable to repair quotas on disk %@ (status code 0x%08X).", context->disk, status );
    }
    else
    {
        DADiskSetState( context->disk, kDADiskStateRequireRepairQuotas, FALSE );
    }

    __DAMountWithArgumentsCallback( 0, context );
}

void DAMount( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.
     */

    return DAMountWithArguments( disk, mountpoint, callback, callbackContext, NULL );

}

/*
 * For a list of arguments in the form of "arg1,arg2,arg3,etc.", check these arguments for mount flags.
 * Each argument is expected to be single strings, with no '=' or other getopt()-adjacent format.
 */
static Boolean __DAMountCheckMntOptsForString( CFMutableStringRef mntOpsStr , CFStringRef argument )
{
    mntoptparse_t mp;
    CFRange result;
    int mntflags = 0;
    int altflags = 0;
    int getmntSilentCurrent;
    uint64_t bufSize;
    char *optionBuffer;
    Boolean containsValue = FALSE;
    static const struct mntopt mopts[] = {
        MOPT_STDOPTS,
        MOPT_UPDATE,
        MOPT_FORCE,
        MOPT_BROWSE,
        { NULL }
    };
    
    bufSize = CFStringGetMaximumSizeForEncoding( CFStringGetLength( mntOpsStr ) ,
                                                 kCFStringEncodingUTF8 );
    
    if ( bufSize == kCFNotFound )
    {
        bufSize = MAXPATHLEN;
    }
    
    optionBuffer = malloc( bufSize );
    
    if ( optionBuffer == NULL )
    {
        DALogError( "Failed to malloc buffer" );
        return FALSE;
    }
    
    if ( CFStringGetCString( mntOpsStr , optionBuffer , bufSize , kCFStringEncodingUTF8 ) == FALSE )
    {
        DALogError( "Failed to copy argument" );
        free( optionBuffer );
        return FALSE;
    }
    
    getmntSilentCurrent = getmnt_silent;
    getmnt_silent = 1; // let getmntopts() return to the caller
    mp = getmntopts( optionBuffer , mopts , &mntflags , &altflags );
    getmnt_silent = getmntSilentCurrent;
    
    if ( mp == NULL )
    {
        DALogError( "Failed to get mnt opts" );
        free( optionBuffer );
        return FALSE;
    }
    
    // Check the "no" and flag-friendly options first before the string searching options
    if ( CFEqual( argument , kDAFileSystemMountArgumentForce ) )
    {
        containsValue = ( mntflags & MNT_FORCE ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoDevice ) )
    {
        containsValue = ( mntflags & MNT_NODEV ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoOwnership )
        || CFEqual( argument , kDAFileSystemMountArgumentNoPermission ) )
    {
        containsValue = ( mntflags & MNT_IGNORE_OWNERSHIP ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoSetUserID ) )
    {
        containsValue = ( mntflags & MNT_NOSUID ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoWrite ) )
    {
        containsValue = ( mntflags & MNT_RDONLY ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentUnion ) )
    {
        containsValue = ( mntflags & MNT_UNION ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentUpdate ) )
    {
        containsValue = ( mntflags & MNT_UPDATE ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoBrowse ) )
    {
        containsValue = ( mntflags & MNT_DONTBROWSE ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoFollow ) )
    {
        containsValue = ( mntflags & MNT_NOFOLLOW ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentNoExecute ) )
    {
        containsValue = ( mntflags & MNT_NOEXEC ) ? TRUE : FALSE;
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentDevice ) )
    {
        // Only return true if the arguments explicitly asked for "dev"
        // Not passing "nodev" is not the same as passing "dev"
        if ( ! ( mntflags & MNT_NODEV ) )
        {
            result = CFStringFind( mntOpsStr , kDAFileSystemMountArgumentDevice , 0 );
            containsValue = ( result.location != kCFNotFound ) ? TRUE : FALSE;
        }
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentOwnership ) )
    {
        // Only return true if the arguments explicitly asked for "owners"
        // Not passing "noowners" is not the same as passing "owners"
        if ( ! ( mntflags & MNT_IGNORE_OWNERSHIP ) )
        {
            result = CFStringFind( mntOpsStr , kDAFileSystemMountArgumentOwnership , 0 );
            containsValue = ( result.location != kCFNotFound ) ? TRUE : FALSE;
        }
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentPermission ) )
    {
        // Only return true if the arguments explicitly asked for "perm"
        // Not passing "noperm" is not the same as passing "perm"
        if ( ! ( mntflags & MNT_IGNORE_OWNERSHIP ) )
        {
            result = CFStringFind( mntOpsStr , kDAFileSystemMountArgumentPermission , 0 );
            containsValue = ( result.location != kCFNotFound ) ? TRUE : FALSE;
        }
    }
    else if ( CFEqual( argument , kDAFileSystemMountArgumentSetUserID ) )
    {
        // Only return true if the arguments explicitly asked for "suid"
        // Not passing "nosuid" is not the same as passing "suid"
        if ( ! ( mntflags & MNT_NOSUID ) )
        {
            result = CFStringFind( mntOpsStr , kDAFileSystemMountArgumentSetUserID , 0 );
            containsValue = ( result.location != kCFNotFound ) ? TRUE : FALSE;
        }
    }
    
    freemntopts( mp );
    free( optionBuffer );
    
    return containsValue;
}

/*
 * Given a single string beginning with '-', check if this string matches the following formats;
 * 1. -o (ex. -onoowners, -oowners=-onoowners)
 * 2. -s (ex. -s=/path/to/snapshot)
 * Return the option supplied by -o, or verify that kDAFileSystemMountArgumentSnapshot is found
 * if that is the argument we are searching for.
 */
static CFStringRef __DAMountGetOpt( CFStringRef optArgStr , CFStringRef argument , Boolean *foundArgument ) {
    char *argv[2];
    char *optArgCStr;
    char opt;
    int oldOptErr = opterr;
    CFStringRef argumentToAdd = NULL;
    Boolean containsValue = FALSE;
    uint64_t bufSize = CFStringGetMaximumSizeForEncoding( CFStringGetLength( optArgStr ) ,
                                                          kCFStringEncodingUTF8 );
    
    if ( bufSize == kCFNotFound )
    {
        bufSize = MAXPATHLEN;
    }
    
    optArgCStr = malloc( bufSize );
    
    if ( optArgCStr != NULL )
    {
        if ( CFStringGetCString( optArgStr , optArgCStr , bufSize ,
                                 kCFStringEncodingUTF8 ) == FALSE )
        {
            DALogError( "Failed to copy option argument" );
            free( optArgCStr );
            return argumentToAdd;
        }
        else
        {
            argv[0] = optArgCStr;
            argv[1] = NULL;
        }
        
        optreset = 1;
        optind = 0;
        opterr = 0;
        while ( !containsValue && ( opt = getopt( 1 , argv , "o:s:") ) != -1 )
        {
            switch ( opt ) {
                case 'o':
                    // Get mount flag
                    argumentToAdd = CFStringCreateWithCString( kCFAllocatorDefault ,
                                                               optarg ,
                                                               kCFStringEncodingUTF8 );
                    break;
                case 's':
                    // Snapshot argument - not a mount flag, but needs to be checked by DA
                    if ( CFEqual( argument , kDAFileSystemMountArgumentSnapshot ) )
                    {
                        containsValue = TRUE;
                        *foundArgument = containsValue;
                    }
                    break;
                default:
                    break;
            }
        }
        opterr = oldOptErr;
        free( optArgCStr );
    }
    
    return argumentToAdd;
}

/*
 * For a list of arguments in the form of "arg1,arg2,arg3,etc.", find the specified argument.
 * Each argument is expected to take one of the following forms:
 * 1. a single string (ex. noowners, nodev, nofollow)
 * 2. an option string that is accepted for getopt() for the following options:
 *    2a. -o (ex. -onoowners, -oowners=-onoowners)
 *    2b. -s (ex. -s=/path/to/snapshot)
 */
Boolean DAMountContainsArgument( CFStringRef arguments, CFStringRef argument )
{
    Boolean containsValue = FALSE;
    CFMutableStringRef mutableArguments = NULL;
    CFArrayRef argumentList;
    CFIndex    argumentListCount;
    CFIndex    argumentListIndex;
    
    if ( arguments == NULL )
    {
        return FALSE;
    }
    
    mutableArguments = CFStringCreateMutable( kCFAllocatorDefault , 0 );
    
    if ( mutableArguments == NULL )
    {
        return FALSE;
    }
    
    /* Process each individual argument at a time, separated by comma. */
    argumentList = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault , arguments , CFSTR( "," ) );

    if ( argumentList )
    {
        argumentListCount = CFArrayGetCount( argumentList );

        for ( argumentListIndex = 0; !containsValue && argumentListIndex < argumentListCount; argumentListIndex++ )
        {
            CFStringRef currentArgument = CFArrayGetValueAtIndex( argumentList, argumentListIndex );
            CFStringRef argumentToAdd = NULL;
            
            if ( currentArgument != NULL )
            {
                /* If this argument starts with a -o or -s, prepare argv to call getopt() */
                if ( CFStringHasPrefix( currentArgument , CFSTR( "-" ) ) )
                {
                    argumentToAdd = __DAMountGetOpt( currentArgument , argument , &containsValue );
                }
                else
                {
                    /* Add this argument verbatim to the list of mount flags to process */
                    argumentToAdd = CFStringCreateCopy( kCFAllocatorDefault, currentArgument );
                }
            }
            
            if ( argumentToAdd != NULL )
            {
                /* Check for argument mapping '(""/owners/perm)=(noowners/noperm)' */
                CFStringRef itemOne;
                CFStringRef itemTwo;
                CFRange result;
                Boolean foundNoowners = FALSE;
                CFArrayRef pair = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault ,
                                                                          argumentToAdd ,
                                                                          CFSTR( "=" ) );
                if ( pair != NULL )
                {
                    /*
                     * Check if the first string is "", "owners", "perm"
                     * and the second string is "noowners" or "noperm".
                     * Treat this argument as "noowners".
                     */
                    if ( CFArrayGetCount( pair ) == 2 )
                    {
                        itemOne = CFArrayGetValueAtIndex( pair , 0 );
                        itemTwo = CFArrayGetValueAtIndex( pair , 1 );
                        
                        if ( CFStringGetLength( itemOne ) == 0
                             || ! CFStringCompare( itemOne ,
                                                   kDAFileSystemMountArgumentOwnership ,
                                                   kCFCompareCaseInsensitive )
                             || ! CFStringCompare( itemOne ,
                                                   kDAFileSystemMountArgumentPermission ,
                                                   kCFCompareCaseInsensitive ) )
                        {
                            result = CFStringFind( itemTwo , kDAFileSystemMountArgumentNoOwnership , 0 );
                            foundNoowners = ( result.location != kCFNotFound );
                            
                            if ( ! foundNoowners )
                            {
                                result = CFStringFind( itemTwo , kDAFileSystemMountArgumentNoPermission , 0 );
                                foundNoowners = ( result.location != kCFNotFound );
                            }
                            
                            if ( foundNoowners )
                            {
                                CFRelease( argumentToAdd );
                                argumentToAdd = CFStringCreateCopy( kCFAllocatorDefault ,
                                                                    kDAFileSystemMountArgumentNoOwnership );
                            }
                        }
                    }
                    CFRelease( pair );
                }
                
                /* If this is not the first argument, separate with a comma */
                if ( CFStringGetLength( mutableArguments ) > 0 )
                {
                    CFStringAppend( mutableArguments , CFSTR(",") );
                }
                CFStringAppend( mutableArguments , argumentToAdd );
                CFRelease( argumentToAdd );
            }
        }
        
        CFRelease( argumentList );
    }
    
    /* Updated argument string now only contains comma separated string suitable for getmntopts() */
    if ( !containsValue && CFStringGetLength( mutableArguments ) > 0 )
    {
        containsValue = __DAMountCheckMntOptsForString( mutableArguments , argument );
    }
        
    CFRelease( mutableArguments );
    
    return containsValue;
}

CFURLRef DAMountCreateMountPoint( DADiskRef disk )
{
    return DAMountCreateMountPointWithAction( disk, kDAMountPointActionMake );
}

CFURLRef DAMountCreateMountPointWithAction( DADiskRef disk, DAMountPointAction action )
{
    CFIndex     index;
    CFURLRef    mountpoint;
    char        name[MAXPATHLEN];
    char        path[MAXPATHLEN];
    char        realMainMountPoint[MAXPATHLEN];
    CFStringRef string;

    mountpoint = NULL;
#if TARGET_OS_OSX
    /*
     * Obtain the volume name.
     */

    string = DADiskGetDescription( disk, kDADiskDescriptionVolumeNameKey );

    if ( string )
    {
        if ( CFStringGetLength( string ) )
        {
            CFRetain( string );
        }
        else
        {
            string = NULL;
        }
    }

    if ( string == NULL )
    {
        string = ___CFBundleCopyLocalizedStringInDirectory( gDABundlePath, CFSTR( "Untitled" ), CFSTR( "Untitled" ), NULL );
    }

    
    if ( ___CFStringGetCString( string, name, MNAMELEN - 20 ) )
    {
        /*
         * Adjust the volume name.
         */

        while ( strchr( name, '/' ) )
        {
            *strchr( name, '/' ) = ':';
        }

        /*
         * Create the mount point path.
         */

        if ( NULL == realpath( kDAMainMountPointFolder, realMainMountPoint) )
        {
            goto exit;
        }
        for ( index = 0; index < 100; index++ )
        {
            if ( index == 0 )
            {
                snprintf( path, sizeof( path ), "%s/%s", realMainMountPoint, name );
            }
            else
            {
                snprintf( path, sizeof( path ), "%s/%s %lu", realMainMountPoint, name, index );
            }

            switch ( action )
            {
                case kDAMountPointActionLink:
                {
                    /*
                     * Link the mount point.
                     */

                    CFURLRef url;

                    url = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

                    if ( url )
                    {
                        char source[MAXPATHLEN];

                        if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) )
                        {
                            if ( symlink( source, path ) == 0 )
                            {
                                mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                            }
                        }
                    }

                    break;
                }
                case kDAMountPointActionMake:
                {
                    /*
                     * Create the mount point.
                     */
///w:start
                    struct statfs fs     = { 0 };
                    int status = statfs( path, &fs );

                    if (status == 0 && strncmp( fs.f_mntonname, kDAMainDataVolumeMountPointFolder, strlen( kDAMainDataVolumeMountPointFolder ) ) == 0 )
                    {
                        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                        if ( mountpoint )
                        {
                            if ( ___CFArrayContainsValue(gDAMountPointList, mountpoint) == FALSE )
                            {
                                DAMountRemoveMountPoint( mountpoint );
                            }
                            CFRelease ( mountpoint );
                            mountpoint = NULL;
                        }
                    }
///w:stop
                    if ( mkdir( path, 0111 ) == 0 )
                    {
                        if ( DADiskGetUserUID( disk ) )
                        {
                            chown( path, DADiskGetUserUID( disk ), -1 );
                        }

                        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                    }

                    break;
                }
                case kDAMountPointActionMove:
                {
                    /*
                     * Move the mount point.
                     */

                    CFURLRef url;

                    url = DADiskGetBypath( disk );

                    if ( url )
                    {
                        char source[MAXPATHLEN];

                        if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) &&
                            strncmp( source, kDAMainMountPointFolder, strlen( kDAMainMountPointFolder ) ) == 0 )
                        {
                            if ( renamex_np( source, path , RENAME_NOFOLLOW_ANY) == 0 )
                            {
                                mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                            }
                        }
                    }

                    break;
                }
                case kDAMountPointActionNone:
                {
                    mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );

                    break;
                }
            }

            if ( mountpoint )
            {
                break;
            }
        }
    }

    CFRelease( string );
#endif

exit:
    return mountpoint;
}

Boolean DAMountGetPreference( DADiskRef disk, DAMountPreference preference )
{
    CFBooleanRef value;

    switch ( preference )
    {
        case kDAMountPreferenceDefer:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey );

                value = value ? value : kCFBooleanTrue;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
            }

            break;
        }
        case kDAMountPreferenceTrust:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey );

                value = value ? value : kCFBooleanFalse;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
            }

            break;
        }
        case kDAMountPreferenceDisableAutoMount:
        {
            /*
            * Determine whether auto mounts are allowed
            */

            value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceAutoMountDisableKey );
#if TARGET_OS_OSX
            value = value ? value : kCFBooleanFalse;
#else
            value = value ? value : kCFBooleanFalse;
#endif

            break;
        }
        case kDAMountPreferenceEnableUserFSMount:
        {
            /*
             * Determine whether the media is removable.
             */
#if TARGET_OS_IOS
            if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanFalse )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountExternalKey );

                value = value ? value : kCFBooleanTrue;
            }
            else
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountInternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountRemovableKey );

                    value = value ? value : kCFBooleanFalse;
                }
            }
#else
            value = kCFBooleanFalse;
#endif
            
            break;
            
        }
        case kDAMountPreferenceAlwaysRepair:
        {
            /*
            * Determine whether we should always run fsck when mounting - used for testing
            */

            value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountAlwaysRepairKey );

            value = value ? value : kCFBooleanFalse;

            break;
        }

        default:
        {
            value = kCFBooleanFalse;

            break;
        }
    }

    assert( value );

    return CFBooleanGetValue( value );
}

void DAMountRemoveMountPoint( CFURLRef mountpoint )
{
    char path[MAXPATHLEN];

    /*
     * Obtain the mount point path.
     */
#if TARGET_OS_OSX
    if ( CFURLGetFileSystemRepresentation( mountpoint, TRUE, ( void * ) path, sizeof( path ) ) )
    {
        if ( ___isautofs( path ) == 0 )
        {
            Boolean remove;
            char       * p = path;

            remove = FALSE;

            if ( strncmp( p, kDAMainDataVolumeMountPointFolder, strlen( kDAMainDataVolumeMountPointFolder ) ) == 0 )
            {
                p += strlen( kDAMainDataVolumeMountPointFolder );
            }

            if ( strncmp( p, kDAMainMountPointFolder, strlen( kDAMainMountPointFolder ) ) == 0 )
            {
                if ( strrchr( p + strlen( kDAMainMountPointFolder ), '/' ) == p + strlen( kDAMainMountPointFolder ) )
                {
                    remove = TRUE;
                }
            }

///w:start
//          if ( remove == FALSE )
///w:stop
            {
                char file[MAXPATHLEN];

                strlcpy( file, path,                              sizeof( file ) );
                strlcat( file, "/",                               sizeof( file ) );
                strlcat( file, kDAMainMountPointFolderCookieFile, sizeof( file ) );

                /*
                 * Remove the mount point cookie file.
                 */

                if ( unlink( file ) == 0 )
                {
                    remove = TRUE;
                }
            }

            if ( remove )
            {
                /*
                 * Remove the mount point.
                 */

                int status = rmdir( path );
                if (status != 0)
                {
                    DALogInfo( "rmdir failed to remove path %s with status %d.", path, errno );
                }
            }
        }
    }
#endif
}

void DAMountWithArguments( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext, ... )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.  All arguments in
     * the argument list shall be of type CFStringRef.  The argument list must be NULL
     * terminated.
     */

    CFStringRef                argument   = NULL;
    va_list                    arguments;
    CFBooleanRef               automatic  = kCFBooleanTrue;
    CFBooleanRef               check      = NULL;
    __DAMountCallbackContext * context    = NULL;
    CFIndex                    count      = 0;
    DAFileSystemRef            filesystem = DADiskGetFileSystem( disk );
    Boolean                    force      = FALSE;
    CFIndex                    index      = 0;
    CFDictionaryRef            map        = NULL;
    CFMutableStringRef         options    = NULL;
    int                        status     = 0;
    CFURLRef                   devicePath = NULL;

    /*
     * Initialize our minimal state.
     */

    if ( mountpoint )
    {
        CFRetain( mountpoint );
    }

    /*
     * Prepare the mount context.
     */

    context = malloc( sizeof( __DAMountCallbackContext ) );

    if ( context == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

    if ( options == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    va_start( arguments, callbackContext );

    while ( ( argument = va_arg( arguments, CFStringRef ) ) )
    {
        if ( CFEqual( argument, kDAFileSystemMountArgumentForce ) )
        {
            force = TRUE;
        }
        else if ( CFEqual( argument, CFSTR( "automatic" ) ) )
        {
            automatic = NULL;

            check = kCFBooleanTrue;
        }
        else
        {
            CFStringAppend( options, argument );
            CFStringAppend( options, CFSTR( "," ) );
        }
    }

    va_end( arguments );

    CFStringTrim( options, CFSTR( "," ) );
///w:start
    context->automatic = ( automatic == NULL ) ? TRUE : FALSE;
///w:stop

    /*
     * no DA mount allowed except apfs preboot volume
     */
    if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceTDMLockedKey ) == kCFBooleanTrue )
    {
        status = EPERM;

///w:start
        /*
         * In the future, use APFSVolumeRole when link with apfs framework can not be avoided.
         */
        if ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) )
        {
            if ( DAAPFSCompareVolumeRole ( disk, CFSTR("PreBoot") ) == TRUE )
            {
                status = 0;
            }
        }
///w:stop

        if ( status )
            goto DAMountWithArgumentsErr;
    }

///w:start

        /*
        * Mount APFS system volumes as read only.
        */
        if ( ( context->automatic == TRUE ) && ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) ) )
        {
            Boolean isSystem = ( DAAPFSCompareVolumeRole ( disk, CFSTR("System") ) == TRUE );
            Boolean noRolePresent =   ( DAAPFSNoVolumeRole ( disk ) == TRUE );
            if ( isSystem == TRUE )
            {
                CFStringInsert( options, 0, CFSTR( "," ) );
                CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );

            }
            
            /*
            * Mount APFS system volumes as nobrowse in base system environment.
            */
#if TARGET_OS_OSX
            if ( os_variant_is_basesystem( "com.apple.diskarbitrationd" ) && ( ( isSystem == TRUE ) || ( noRolePresent == TRUE ) ) )
            {
                CFStringInsert( options, 0, CFSTR( "," ) );
                CFStringInsert( options, 0, kDAFileSystemMountArgumentNoBrowse );
            }
#endif
        }
///w:stop

    /*
    * Mount volumes with "quarantine" ioreg property with quarantine flag
    */
    if ( ( context->automatic == TRUE ) && ( DADiskGetState( disk, _kDADiskStateMountQuarantined ) ) )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, CFSTR( "quarantine" ) );
    }
    
    /*
     * Determine whether the volume is to be updated.
     */

    if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentUpdate ) )
    {
        if ( mountpoint )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

        if ( mountpoint == NULL )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        CFRetain( mountpoint );
    }

    if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentSnapshot ) )
    {
        if ( mountpoint == NULL )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        devicePath = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

        if ( devicePath == NULL )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }
    }
    
    else
    {
        devicePath = DADiskGetDevice( disk );
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList1 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList1, index );

        if ( map )
        {
            CFTypeRef   id;
            CFStringRef kind;

            id   = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );
            kind = CFDictionaryGetValue( map, kDAMountMapProbeKindKey );

            if ( kind )
            {
                /*
                 * Determine whether the volume kind matches.
                 */

                if ( CFEqual( kind, DAFileSystemGetKind( filesystem ) ) == FALSE )
                {
                    continue;
                }
            }

            if ( CFGetTypeID( id ) == CFUUIDGetTypeID( ) )
            {
                /*
                 * Determine whether the volume UUID matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFStringGetTypeID( ) )
            {
                /*
                 * Determine whether the volume name matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFDictionaryGetTypeID( ) )
            {
                boolean_t match = FALSE;

                /*
                 * Determine whether the device description matches.
                 */

                IOServiceMatchPropertyTable( DADiskGetIOMedia( disk ), id, &match );

                if ( match )
                {
                    break;
                }
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Determine whether the volume is to be mounted.
         */

        if ( automatic == NULL )
        {
            automatic = CFDictionaryGetValue( map, kDAMountMapMountAutomaticKey );

            if ( automatic == kCFBooleanTrue )
            {
                DADiskSetState( disk, _kDADiskStateMountAutomatic,        TRUE );
                DADiskSetState( disk, _kDADiskStateMountAutomaticNoDefer, TRUE );
            }
        }

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }

        /*
         * Prepare the mount point.
         */

        if ( mountpoint == NULL )
        {
            mountpoint = CFDictionaryGetValue( map, kDAMountMapMountPathKey );

            if ( mountpoint )
            {
                CFRetain( mountpoint );
            }
        }
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList2 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList2, index );

        if ( map )
        {
            CFTypeRef id;

            id = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );

            /*
             * Determine whether the volume UUID matches.
             */

            if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
            {
                break;
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }
    }

    /*
     * Determine whether the volume is to be mounted.
     */

    if ( automatic == NULL )
    {
        if ( DADiskGetState( disk, _kDADiskStateMountAutomatic ) )
        {
            if ( DADiskGetState( disk, _kDADiskStateMountAutomaticNoDefer ) )
            {
                automatic = kCFBooleanTrue;
            }
        }
        else
        {
            automatic = kCFBooleanFalse;
        }

        if ( automatic == NULL )
        {
#if TARGET_OS_OSX
            if ( gDAConsoleUserList == NULL )
#elif TARGET_OS_IOS
            if ( gDAUnlockedState == FALSE )         
#endif
            {
                if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                {
                    DALogInfo( " No console users yet, delaying mount of %@", disk );

                    automatic = kCFBooleanFalse;
                }
            }
        }
    }

    if ( automatic == kCFBooleanFalse )
    {
        status = ECANCELED;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
    }

    if ( DAMountGetPreference( disk, kDAMountPreferenceTrust ) == FALSE )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoSetUserID );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoOwnership );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoDevice );
    }
    
///w:start
    if ( CFEqual( DAFileSystemGetKind( filesystem ), CFSTR( "hfs" ) ) )
    {
        ___CFStringInsertFormat( options, 0, CFSTR( "-m=%o," ), 0755 );

        if ( DADiskGetUserGID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), DADiskGetUserGID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), ___GID_UNKNOWN );
        }

        if ( DADiskGetUserUID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), DADiskGetUserUID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), ___UID_UNKNOWN );
        }
    }
///w:stop

    CFStringTrim( options, CFSTR( "," ) );
    
    DALogInfo(" Mount options %@", options);
    /*
     * Determine whether the volume is to be repaired.
     */

    if ( check == NULL )
    {
        if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentNoWrite ) )
        {
            check = kCFBooleanFalse;
        }
        else
        {
            check = kCFBooleanTrue;
        }
    }

    if ( check == kCFBooleanFalse )
    {
        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
        {
            if ( force == FALSE )
            {
                status = ___EDIRTY;

                goto DAMountWithArgumentsErr;
            }
        }
    }

    if ( check == kCFBooleanTrue )
    {
        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) == FALSE )
        {
            check = kCFBooleanFalse;
        }
    }
    
    // Check if the preference to always run fsck is set
    if ( check == kCFBooleanFalse && DAMountGetPreference( disk , kDAMountPreferenceAlwaysRepair ) == TRUE )
    {
        check = kCFBooleanTrue;
    }

    /*
     * Repair the volume.
     */

    CFRetain( disk );

    context->assertionID     = kIOPMNullAssertionID;
    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->disk            = disk;
    context->force           = force;
    context->mountpoint      = mountpoint;
    context->options         = options;
    context->devicePath      = devicePath;
    context->contDisk        = NULL;
    context->fd              = -1;
    
    if ( check == kCFBooleanTrue )
    {
#if TARGET_OS_IOS
        context->contDisk = DADiskGetContainerDisk( disk );
        if ( context->contDisk )
        {
            int fd = DAUserFSOpen( DADiskGetBSDPath( context->contDisk, TRUE ), O_RDWR );
            if ( fd == -1 )
            {
                status = errno;
                
                goto DAMountWithArgumentsErr;
                
            }
            DAUnitSetState( context->contDisk, kDAUnitStateCommandActive, TRUE );
            CFRetain( context->contDisk );
            int newfd = dup (fd );
            close (fd);
            context->fd = newfd;
        }
        else
        {
            int fd = DAUserFSOpen(DADiskGetBSDPath( disk, TRUE), O_RDWR);
            if ( fd == -1 )
            {
                status = errno;
                
                goto DAMountWithArgumentsErr;
                
            }
            int newfd = dup (fd );
            close (fd);
            context->fd = newfd;
        }
#endif
        DALogInfo( "repaired disk, id = %@, ongoing.", disk );

        DADiskSetDescription( disk , kDADiskDescriptionRepairRunningKey , kCFBooleanTrue );
        DADiskDescriptionChangedCallback( disk , kDADiskDescriptionRepairRunningKey );
        
        IOPMAssertionCreateWithDescription( kIOPMAssertionTypePreventUserIdleSystemSleep,
                                            CFSTR( _kDADaemonName ),
                                            NULL,
                                            NULL,
                                            NULL,
                                            0,
                                            NULL,
                                            &context->assertionID );
        context->fsckStartTime = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
        DAFileSystemRepair( DADiskGetFileSystem( disk ),
                           (context->contDisk)? DADiskGetDevice( context->contDisk ):DADiskGetDevice( disk ),
                            context->fd,
                            __DAMountWithArgumentsCallbackStage1,
                            context );
    }
    else
    {
        __DAMountWithArgumentsCallbackStage1( ECANCELED, context );
    }

DAMountWithArgumentsErr:

    if ( status )
    {
        if ( context )
        {
            free( context );
        }

        if ( mountpoint )
        {
            CFRelease( mountpoint );
        }

        if ( options )
        {
            CFRelease( options );
        }

        if ( callback )
        {
            ( callback )( status, NULL, callbackContext );
        }
    }
}
