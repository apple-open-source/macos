/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
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

#include "DAProbe.h"

#include "DALog.h"
#include "DAMain.h"
#include "DASupport.h"

#include <fsproperties.h>
#include <sys/loadable_fs.h>
#include <os/feature_private.h>

static void __DAProbeCallback( int status, int cleanStatus, CFStringRef name, CFStringRef type, CFUUIDRef uuid, void * parameter )
{
    /*
     * Process the probe command's completion.
     */

    __DAProbeCallbackContext * context = parameter;
    bool doFsck                       = true;
    char *containerBSDPath            = NULL;
    
    if ( status )
    {
        /*
         * We have found no probe match for this media object.
         */

        if ( context->filesystem )
        {
            CFStringRef kind;

            kind = DAFileSystemGetKind( context->filesystem );

            DALogInfo( "probed disk, id = %@, with %@, failure.", context->disk, kind );

            if ( status != FSUR_UNRECOGNIZED )
            {
                DALogError( "unable to probe %@ (status code 0x%08X).", context->disk, status );
            }

            CFRelease( context->filesystem );

            context->filesystem = NULL;
        }

#if !TARGET_OS_OSX
        if ( ( ( DADiskGetDescription( context->disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue ) &&
            ( DADiskGetDescription( context->disk, kDADiskDescriptionDeviceInternalKey ) == NULL ) ) ||
            ( DADiskGetDescription( context->disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue ) )
        {
            doFsck = false;
        }
#endif
        /*
         * Find a probe candidate for this media object.
         */

        while ( CFArrayGetCount( context->candidates ) )
        {
            CFDictionaryRef candidate;

            candidate = CFArrayGetValueAtIndex( context->candidates, 0 );

            if ( candidate )
            {
                DAFileSystemRef filesystem;

                filesystem = ( void * ) CFDictionaryGetValue( candidate, kDAFileSystemKey );

                if ( filesystem )
                {
                    CFDictionaryRef properties;

                    properties = CFDictionaryGetValue( candidate, CFSTR( kFSMediaPropertiesKey ) );

                    if ( properties )
                    {
                        boolean_t match = FALSE;

                        IOServiceMatchPropertyTable( DADiskGetIOMedia( context->disk ), properties, &match );

                        if ( match )
                        {
                            /*
                             * We have found a probe candidate for this media object.
                             */

                            CFStringRef kind;

                            kind = DAFileSystemGetKind( filesystem );

                            CFRetain( filesystem );

                            context->filesystem = filesystem;

                            if ( CFDictionaryGetValue( candidate, CFSTR( "autodiskmount" ) ) == kCFBooleanFalse )
                            {
                                DADiskSetState( context->disk, _kDADiskStateMountAutomatic,        FALSE );
                                DADiskSetState( context->disk, _kDADiskStateMountAutomaticNoDefer, FALSE );
                            }

                            CFArrayRemoveValueAtIndex( context->candidates, 0 );

                            DALogInfo( "probed disk, id = %@, with %@, ongoing.", context->disk, kind );

#if TARGET_OS_IOS
                            if ( context->containerDisk )
                            {
                                containerBSDPath = DADiskGetBSDPath( context->containerDisk, TRUE);
                            }
                            else
                            {
                                containerBSDPath = NULL;
                            }
#endif
                            
                            DAFileSystemProbe( filesystem, DADiskGetDevice( context->disk ), DADiskGetBSDPath( context->disk, TRUE ), containerBSDPath, __DAProbeCallback, context, doFsck );

                            return;
                        }
                    }
                }
            }

            CFArrayRemoveValueAtIndex( context->candidates, 0 );
        }
        
#ifdef DA_FSKIT
        /*
         * Only probe the FSModules for the current logged in user if we have the preference for it
         * and don't have any other viable candidates.
         */
        if (!gFSKitMissing
            &&  os_feature_enabled( DiskArbitration, enableFSKitModules ) 
            &&  !context->gotFSModules ) {
            context->gotFSModules = 1;
            
            /* Allocate independent context here to account for asynchronous operation */
            __DAProbeCallbackContext *contextCopy = malloc( sizeof( __DAProbeCallbackContext ) );
            
            if ( contextCopy )
            {
                CFRetain( context->disk );
                
                contextCopy->callback        = context->callback;
                contextCopy->callbackContext = context->callbackContext;
                contextCopy->candidates      = NULL; /* non-FSKit candidates are not needed here */
                contextCopy->disk            = context->disk;
                contextCopy->containerDisk   = context->containerDisk;
                contextCopy->filesystem      = NULL; /* begin our own callback cycle with FSKit */
                contextCopy->gotFSModules    = 1;
                
                DAGetFSModulesForUser( gDAConsoleUserUID , contextCopy );
            }
        }
#endif
    
    }
    else
    {
        /*
         * We have found a probe match for this media object.
         */

        CFStringRef kind;

        kind = DAFileSystemGetKind( context->filesystem );

        DALogInfo( "probed disk, id = %@, with %@, success.", context->disk, kind );
    }
    
    if ( context->callback 
#ifdef DA_FSKIT
        /* Don't call the callback if we haven't succeeded probe or before we query FSModules if FSKit is available */
        && ( !status || !context->gotFSModules )
#endif
        )
    {
        ( context->callback )( status, context->filesystem, cleanStatus, name, type, uuid, context->callbackContext );
    }

    CFRelease( context->candidates );
    CFRelease( context->disk       );
#if TARGET_OS_IOS
    if ( context->containerDisk )
    {
        DAUnitSetState( context->containerDisk, kDAUnitStateCommandActive, FALSE );
        CFRelease( context->containerDisk       );
    }
#endif
    if ( context->filesystem )  CFRelease( context->filesystem );

    free( context );
}

void DAProbe( DADiskRef disk, DADiskRef containerDisk, DAProbeCallback callback, void * callbackContext )
{
    /*
     * Probe the specified volume.  A status of 0 indicates success.
     */

    CFMutableArrayRef          candidates = NULL;
    __DAProbeCallbackContext * context    = NULL;
    CFNumberRef                size       = NULL;
    int                        status     = 0;

    /*
     * Prepare the probe context.
     */

    context = malloc( sizeof( __DAProbeCallbackContext ) );

    if ( context == NULL )
    {
        status = ENOMEM;

        goto DAProbeErr;
    }

    /*
     * Prepare the probe candidates.
     */

    candidates = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, gDAFileSystemProbeList );

    if ( candidates == NULL )
    {
        status = ENOMEM;

        goto DAProbeErr;
    }

    /*
     * Determine whether the disk is formatted.
     */

    size = DADiskGetDescription( disk, kDADiskDescriptionMediaSizeKey );

    if ( size )
    {
        if ( ___CFNumberGetIntegerValue( size ) == 0 )
        {
            CFArrayRemoveAllValues( candidates );
        }
    }

    /*
     * Probe the volume.
     */

    CFRetain( disk );
#if TARGET_OS_IOS
    if ( containerDisk )
    {
        DAUnitSetState( containerDisk, kDAUnitStateCommandActive, TRUE );
        CFRetain( containerDisk );
    }
#endif
    
    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->candidates      = candidates;
    context->disk            = disk;
    context->containerDisk   = containerDisk;
    context->filesystem      = NULL;
#ifdef DA_FSKIT
    context->gotFSModules = 0;
#endif
    
    __DAProbeCallback( -1, NULL, NULL, NULL, NULL, context );

DAProbeErr:

    if ( status )
    {

        if ( candidates )
        {
            CFRelease( candidates );
        }

        if ( context )
        {
            free( context );
        }

        if ( callback )
        {
            ( callback )( status, -1, NULL, NULL, NULL, NULL, callbackContext );
        }
    }
}
