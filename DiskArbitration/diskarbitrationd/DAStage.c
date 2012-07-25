/*
 * Copyright (c) 1998-2011 Apple Inc. All Rights Reserved.
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

#include "DAStage.h"

#include "DABase.h"
#include "DACallback.h"
#include "DADialog.h"
#include "DADisk.h"
#include "DAFileSystem.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAMount.h"
#include "DAPrivate.h"
#include "DAQueue.h"
#include "DASupport.h"

#include <fsproperties.h>
#include <unistd.h>
#include <sys/loadable_fs.h>
#include <sys/mount.h>
#include <IOKit/storage/IOMedia.h>

static CFRunLoopSourceRef __gDAStageRunLoopSource = NULL;

static void               __DAStageAppeared( DADiskRef disk );
static void               __DAStageMount( DADiskRef disk );
static void               __DAStageMountCallback( int status, CFURLRef mountpoint, void * context );
static void               __DAStageMountApproval( DADiskRef disk );
static void               __DAStageMountApprovalCallback( CFTypeRef response, void * context );
static void               __DAStageMountAuthorization( DADiskRef disk );
static void               __DAStageMountAuthorizationCallback( DAReturn status, void * context );
static void               __DAStagePeek( DADiskRef disk );
static void               __DAStagePeekCallback( CFTypeRef response, void * context );
static CFComparisonResult __DAStagePeekCompare( const void * value1, const void * value2, void * context );
static void               __DAStageProbe( DADiskRef disk );
static void               __DAStageProbeCallback( int status, CFBooleanRef clean, CFStringRef name, CFUUIDRef uuid, void * context );
static void               __DAStageRepair( DADiskRef disk );
static void               __DAStageRepairCallback( int status, void * context );

static void __DAStageAppeared( DADiskRef disk )
{
    /*
     * We commence the "appeared" stage if the conditions are right.
     */

    DADiskSetState( disk, kDADiskStateStagedAppear, TRUE );
    
    DADiskAppearedCallback( disk );

    DAStageSignal( );
}

static void __DAStageDispatch( void * info )
{
    static Boolean fresh = FALSE;

    CFIndex count;
    CFIndex index;
    Boolean quiet = TRUE;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( DADiskGetState( disk, kDADiskStateCommandActive ) == FALSE )
        {
            if ( DADiskGetState( disk, kDADiskStateStagedProbe ) == FALSE )
            {
                if ( fresh )
                {
                    DAFileSystemListRefresh( );

                    DAMountMapListRefresh1( );
                    
                    DAMountMapListRefresh2( );

                    fresh = FALSE;
                }

                __DAStageProbe( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedPeek ) == FALSE )
            {
                __DAStagePeek( disk );
            }
///w:start
            else if ( DADiskGetState( disk, kDADiskStateRequireRepair ) == FALSE )
            {
                if ( DADiskGetState( disk, kDADiskStateStagedRepair ) == FALSE )
                {
                    __DAStageRepair( disk );
                }
                else if ( DADiskGetState( disk, kDADiskStateStagedApprove ) == FALSE )
                {
                    __DAStageMountApproval( disk );
                }
                else if ( DADiskGetState( disk, kDADiskStateStagedAuthorize ) == FALSE )
                {
                    __DAStageMountAuthorization( disk );
                }
                else if ( DADiskGetState( disk, kDADiskStateStagedMount ) == FALSE )
                {
                    __DAStageMount( disk );
                }
                else if ( DADiskGetState( disk, kDADiskStateStagedAppear ) == FALSE )
                {
                    __DAStageAppeared( disk );
                }
                else
                {
///w:start
                    if ( gDAConsoleUserList == NULL )
                    {
                        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
                        {
                            if ( DAUnitGetState( disk, kDAUnitStateStagedUnreadable ) == FALSE )
                            {
                                if ( _DAUnitIsUnreadable( disk ) )
                                {
                                    DADiskEject( disk, kDADiskEjectOptionDefault, NULL );
                                }

                                DAUnitSetState( disk, kDAUnitStateStagedUnreadable, TRUE );
                            }
                        }
                    }
///w:stop
                    continue;
                }
            }
///w:stop
            else if ( DADiskGetState( disk, kDADiskStateStagedAppear ) == FALSE )
            {
///w:start
                /*
                 * We stall the "appeared" stage if the conditions are not right.
                 */

                if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
                {
                    CFIndex subcount;
                    CFIndex subindex;

                    subcount = CFArrayGetCount( gDADiskList );

                    for ( subindex = 0; subindex < subcount; subindex++ )
                    {
                        DADiskRef subdisk;

                        subdisk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, subindex );

                        if ( DADiskGetBSDUnit( disk ) == DADiskGetBSDUnit( subdisk ) )
                        {
                            if ( DADiskGetState( subdisk, kDADiskStateStagedProbe ) == FALSE )
                            {
                                break;
                            }

                            if ( DADiskGetState( subdisk, kDADiskStateStagedAppear ) == FALSE )
                            {
                                if ( DADiskGetState( subdisk, kDADiskStateRequireRepair ) == FALSE )
                                {
                                    break;
                                }
                            }
                        }
                    }

                    if ( subindex == subcount )
                    {
                        __DAStageAppeared( disk );

                        __DAStageRepair( disk );
                    }
                }
                else
///w:stop
                __DAStageAppeared( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedRepair ) == FALSE )
            {
                /*
                 * We stall the "repair" stage if the conditions are not right.
                 */

                if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
                {
                    CFIndex subcount;
                    CFIndex subindex;

                    subcount = CFArrayGetCount( gDADiskList );

                    for ( subindex = 0; subindex < subcount; subindex++ )
                    {
                        DADiskRef subdisk;

                        subdisk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, subindex );

                        if ( DADiskGetBSDUnit( disk ) == DADiskGetBSDUnit( subdisk ) )
                        {
                            if ( DADiskGetState( subdisk, kDADiskStateStagedProbe ) == FALSE )
                            {
                                break;
                            }

                            if ( DADiskGetState( subdisk, kDADiskStateStagedMount ) == FALSE )
                            {
                                if ( DADiskGetState( subdisk, kDADiskStateRequireRepair ) == FALSE )
                                {
                                    break;
                                }
                            }
                        }
                    }

                    if ( subindex == subcount )
                    {
                        if ( gDAExit )
                        {
                            continue;
                        }

                        __DAStageRepair( disk );
                    }
                }
                else
                {
                    __DAStageRepair( disk );
                }
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedApprove ) == FALSE )
            {
                __DAStageMountApproval( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedAuthorize ) == FALSE )
            {
                __DAStageMountAuthorization( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedMount ) == FALSE )
            {
                __DAStageMount( disk );
            }
            else
            {
                continue;
            }
        }

        quiet = FALSE;
    }

    count = CFArrayGetCount( gDARequestList );

    if ( count )
    {
        CFMutableSetRef dependencies;

        dependencies = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );

        if ( dependencies )
        {
            for ( index = 0; index < count; index++ )
            {
                DARequestRef request;

                request = ( void * ) CFArrayGetValueAtIndex( gDARequestList, index );

                if ( request )
                {
                    DADiskRef disk;
                    Boolean   dispatch = TRUE;

                    disk = DARequestGetDisk( request );

                    /*
                     * Determine whether the request has undispatched dependencies.
                     */

                    if ( disk )
                    {
                        CFArrayRef link;

                        link = DARequestGetLink( request );

                        if ( link )
                        {
                            CFIndex subcount;
                            CFIndex subindex;

                            subcount = CFArrayGetCount( link );

                            for ( subindex = 0; subindex < subcount; subindex++ )
                            {
                                DARequestRef subrequest;

                                subrequest = ( void * ) CFArrayGetValueAtIndex( link, subindex );

                                if ( CFSetContainsValue( dependencies, DARequestGetDisk( subrequest ) ) )
                                {
                                    break;
                                }
                            }

                            if ( subindex < subcount )
                            {
                                dispatch = FALSE;
                            }
                        }

                        if ( CFSetContainsValue( dependencies, disk ) )
                        {
                            dispatch = FALSE;
                        }
                    }
                    else
                    {
                        if ( index )
                        {
                            break;
                        }
                    }

                    if ( dispatch )
                    {
                        /*
                         * Prepare to dispatch the request.
                         */

                        if ( DARequestGetKind( request ) == _kDADiskMount )
                        {
                            if ( fresh )
                            {
                                DAFileSystemListRefresh( );

                                DAMountMapListRefresh1( );

                                DAMountMapListRefresh2( );

                                fresh = FALSE;
                            }
                        }

                        /*
                         * Dispatch the request.
                         */

                        dispatch = DARequestDispatch( request );
                    }

                    if ( dispatch )
                    {
                        CFArrayRemoveValueAtIndex( gDARequestList, index );

                        count--;
                        index--;
                    }
                    else
                    {
                        /*
                         * Add the request to the undispatched dependencies.
                         */

                        if ( disk )
                        {
                            CFArrayRef link;

                            link = DARequestGetLink( request );

                            if ( link )
                            {
                                CFIndex subcount;
                                CFIndex subindex;

                                subcount = CFArrayGetCount( link );

                                for ( subindex = 0; subindex < subcount; subindex++ )
                                {
                                    DARequestRef subrequest;

                                    subrequest = ( void * ) CFArrayGetValueAtIndex( link, subindex );

                                    CFSetSetValue( dependencies, DARequestGetDisk( subrequest ) );
                                }
                            }

                            CFSetSetValue( dependencies, disk );
                        }
                    }
                }
            }

            CFRelease( dependencies );
        }

        quiet = FALSE;
    }

    if ( quiet )
    {
        fresh = TRUE;

        gDAIdle = TRUE;

        DAIdleCallback( );

        ___vproc_transaction_end( );

        if ( gDAConsoleUser )
        {
            /*
             * Determine whether a unit is unreadable or a volume is unrepairable.
             */

            count = CFArrayGetCount( gDADiskList );

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                /*
                 * Determine whether a unit is unreadable.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
                {
                    if ( DAUnitGetState( disk, kDAUnitStateStagedUnreadable ) == FALSE )
                    {
                        if ( _DAUnitIsUnreadable( disk ) )
                        {
                            DADialogShowDeviceUnreadable( disk );
                        }

                        DAUnitSetState( disk, kDAUnitStateStagedUnreadable, TRUE );
                    }
                }

                /*
                 * Determine whether a volume is unrepairable.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
                {
                    if ( DADiskGetState( disk, kDADiskStateStagedUnrepairable ) == FALSE )
                    {
                        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
                        {
                            if ( DADiskGetOption( disk, kDADiskOptionMountAutomatic ) )
                            {
                                if ( DADiskGetClaim( disk ) == NULL )
                                {
                                    DADialogShowDeviceUnrepairable( disk );
                                }
                            }
                        }

                        DADiskSetState( disk, kDADiskStateStagedUnrepairable, TRUE );
                    }
                }
            }
        }
    }
}

static void __DAStageMount( DADiskRef disk )
{
    /*
     * We commence the "mount" stage if the conditions are right.
     */

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( DAUnitGetState( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        /*
         * Commence the mount.
         */

        CFRetain( disk );

        DADiskSetState( disk, kDADiskStateStagedMount, TRUE );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

        DALogDebug( "  mounted disk, id = %@, ongoing.", disk );

        DAMountWithArguments( disk, NULL, __DAStageMountCallback, disk, CFSTR( "automatic" ), NULL );
    }
}

static void __DAStageMountCallback( int status, CFURLRef mountpoint, void * context )
{
    DADiskRef disk = context;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( status )
    {
        /*
         * We were unable to mount the volume.
         */

        if ( status == ECANCELED )
        {
            DALogDebug( "  mounted disk, id = %@, deferred.", disk );
        }
        else
        {
            DALogDebug( "  mounted disk, id = %@, failure.", disk );

            DALogError( "unable to mount %@ (status code 0x%08X).", disk, status );
        }
    }
    else
    {
        /*
         * We were able to mount the volume.
         */

        DADiskSetBypath( disk, mountpoint );

        DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, mountpoint );

        DALogDebug( "  mounted disk, id = %@, success.", disk );

        if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
        {
            DADiskDescriptionChangedCallback( disk, kDADiskDescriptionVolumePathKey );
        }
    }

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( disk );
}

static void __DAStageMountApproval( DADiskRef disk )
{
    /*
     * We commence the "mount approval" stage if the conditions are right.
     */

    Boolean mount = TRUE;

    /*
     * Determine whether the disk is mountable.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
    {
        mount = FALSE;
    }

    /*
     * Determine whether the disk is mounted.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
    {
        mount = FALSE;
    }

    /*
     * Commence the mount approval.
     */

    DADiskSetState( disk, kDADiskStateStagedApprove, TRUE );

    if ( mount )
    {
        CFRetain( disk );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DADiskMountApprovalCallback( disk, __DAStageMountApprovalCallback, disk );
    }
    else
    {
        DADiskSetState( disk, kDADiskStateStagedMount, TRUE );

        DAStageSignal( );
    }
}

static void __DAStageMountApprovalCallback( CFTypeRef response, void * context )
{
    DADiskRef      disk      = context;
    DADissenterRef dissenter = response;

    if ( dissenter )
    {
        /*
         * The mount was disapproved.
         */

///w:start
        if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF01 )
        {
            DADiskSetState( disk, _kDADiskStateRequireAuthorize, TRUE );
        }
        else if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF02 )
        {
            DADiskSetState( disk, _kDADiskStateMountPreferenceNoWrite, TRUE );
        }
        else if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF03 )
        {
            DADiskSetState( disk, _kDADiskStateRequireAuthorize, TRUE );

            DADiskSetState( disk, _kDADiskStateMountPreferenceNoWrite, TRUE );
        }
        else
///w:stop
        DADiskSetState( disk, kDADiskStateStagedMount, TRUE );
    }

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( disk );
}

static void __DAStageMountAuthorization( DADiskRef disk )
{
    /*
     * We commence the "mount authorization" stage if the conditions are right.
     */

    /*
     * Commence the mount authorization.
     */

    DADiskSetState( disk, kDADiskStateStagedAuthorize, TRUE );

///w:start
    if ( DADiskGetState( disk, _kDADiskStateRequireAuthorize ) )
///w:stop
    {
        CFRetain( disk );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAAuthorizeWithCallback( NULL,
                                 _kDAAuthorizeOptionAuthenticateAdministrator,
                                 disk,
                                 ___UID_ROOT,
                                 ___GID_WHEEL,
                                 __DAStageMountAuthorizationCallback,
                                 disk,
                                 _kDAAuthorizeRightMount );
    }
    else
    {
        DAStageSignal( );
    }
}

static void __DAStageMountAuthorizationCallback( DAReturn status, void * context )
{
    DADiskRef disk = context;

    if ( status )
    {
        /*
         * The mount was unauthorized.
         */

        DADiskSetState( disk, kDADiskStateStagedMount, TRUE );
    }

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( disk );
}

static void __DAStagePeek( DADiskRef disk )
{
    /*
     * We commence the "peek" stage if the conditions are right.
     */

    CFMutableArrayRef candidates;

    candidates = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    if ( candidates )
    {
        DASessionRef session;
        CFArrayRef   sessionList;
        CFIndex      sessionListCount;
        CFIndex      sessionListIndex;

        sessionList      = gDASessionList;
        sessionListCount = CFArrayGetCount( sessionList );

        for ( sessionListIndex = 0; sessionListIndex < sessionListCount; sessionListIndex++ )
        {
            DACallbackRef callback;
            CFArrayRef    callbackList;
            CFIndex       callbackListCount;
            CFIndex       callbackListIndex;
            
            session = ( void * ) CFArrayGetValueAtIndex( sessionList, sessionListIndex );

            callbackList      = DASessionGetCallbackRegister( session );
            callbackListCount = CFArrayGetCount( callbackList );

            for ( callbackListIndex = 0; callbackListIndex < callbackListCount; callbackListIndex++ )
            {
                callback = ( void * ) CFArrayGetValueAtIndex( callbackList, callbackListIndex );

                if ( DACallbackGetKind( callback ) == _kDADiskPeekCallback )
                {
                    CFArrayAppendValue( candidates, callback );
                }
            }
        }

        CFArraySortValues( candidates, CFRangeMake( 0, CFArrayGetCount( candidates ) ), __DAStagePeekCompare, NULL );

        /*
         * Commence the peek.
         */

        CFRetain( disk );

        DADiskSetContext( disk, candidates );

        DADiskSetState( disk, kDADiskStateStagedPeek, TRUE );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        __DAStagePeekCallback( NULL, disk );

        CFRelease( candidates );
    }
}

static void __DAStagePeekCallback( CFTypeRef response, void * context )
{
    CFMutableArrayRef candidates;
    DADiskRef         disk = context;

    candidates = ( void * ) DADiskGetContext( disk );

    if ( CFArrayGetCount( candidates ) )
    {
        DACallbackRef candidate;

        candidate = ( void * ) CFArrayGetValueAtIndex( candidates, 0 );

        CFRetain( candidate );

        CFArrayRemoveValueAtIndex( candidates, 0 );

        DADiskPeekCallback( disk, candidate, __DAStagePeekCallback, context );

        CFRelease( candidate );

        return;
    }
    
    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DADiskSetContext( disk, NULL );

    DAStageSignal( );

    CFRelease( disk );
}

static CFComparisonResult __DAStagePeekCompare( const void * value1, const void * value2, void * context )
{
    SInt32 order1 = DACallbackGetOrder( ( void * ) value1 );
    SInt32 order2 = DACallbackGetOrder( ( void * ) value2 );

    if ( order1 > order2 )  return kCFCompareGreaterThan;
    if ( order1 < order2 )  return kCFCompareLessThan;

    return kCFCompareEqualTo;
}

static void __DAStageProbe( DADiskRef disk )
{
    /*
     * We commence the "probe" stage if the conditions are right.
     */

    if ( DAUnitGetState( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        CFMutableArrayRef candidates;

        candidates = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, gDAFileSystemProbeList );

        if ( candidates )
        {
            CFNumberRef size;

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
             * Commence the probe.
             */

            CFRetain( disk );

            DADiskSetFileSystem( disk, NULL );

            DADiskSetContext( disk, candidates );

            DADiskSetState( disk, kDADiskStateStagedProbe, TRUE );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

            __DAStageProbeCallback( -1, NULL, NULL, NULL, disk );

            CFRelease( candidates );
        }
    }
}

static void __DAStageProbeCallback( int status, CFBooleanRef clean, CFStringRef name, CFUUIDRef uuid, void * context )
{
    DADiskRef         disk = context;
    CFMutableArrayRef keys = NULL;
    CFStringRef       kind = NULL;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( status )
    {
        CFMutableArrayRef candidates;

        candidates = ( void * ) DADiskGetContext( disk );

        if ( DADiskGetFileSystem( disk ) )
        {
            kind = DAFileSystemGetKind( DADiskGetFileSystem( disk ) );

            DALogDebug( "  probed disk, id = %@, with %@, failure.", disk, kind );

            if ( status != FSUR_UNRECOGNIZED )
            {
                DALogError( "unable to probe %@ (status code 0x%08X).", disk, status );
            }
        }

        /*
         * Find a probe candidate for this media object.
         */

        while ( CFArrayGetCount( candidates ) )
        {
            CFDictionaryRef candidate;

            candidate = CFArrayGetValueAtIndex( candidates, 0 );

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

                        IOServiceMatchPropertyTable( DADiskGetIOMedia( disk ), properties, &match );

                        if ( match )
                        {
                            /*
                             * We have found a probe candidate for this media object.
                             */

                            kind = DAFileSystemGetKind( filesystem );

                            DADiskSetFileSystem( disk, filesystem );

                            if ( CFDictionaryGetValue( candidate, CFSTR( "autodiskmount" ) ) == kCFBooleanFalse )
                            {
                                DADiskSetOption( disk, kDADiskOptionMountAutomatic,        FALSE );
                                DADiskSetOption( disk, kDADiskOptionMountAutomaticNoDefer, FALSE );
                            }

                            CFArrayRemoveValueAtIndex( candidates, 0 );

                            DALogDebug( "  probed disk, id = %@, with %@, ongoing.", disk, kind );

                            DAFileSystemProbe( filesystem, DADiskGetDevice( disk ), __DAStageProbeCallback, context );

                            return;
                        }
                    }
                }
            }

            CFArrayRemoveValueAtIndex( candidates, 0 );
        }
    }

    DADiskSetState( disk, kDADiskStateRequireRepair,       FALSE );
    DADiskSetState( disk, kDADiskStateRequireRepairQuotas, FALSE );

    if ( status )
    {
        /*
         * We have found no probe match for this media object.
         */

        kind = NULL;

        if ( DADiskGetFileSystem( disk ) )
        {
            DADiskSetFileSystem( disk, NULL );

            DALogDebug( "  probed disk, id = %@, no match.", disk );
        }
    }
    else
    {
        /*
         * We have found a probe match for this media object.
         */

        kind = DAFileSystemGetKind( DADiskGetFileSystem( disk ) );

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
        {
            clean = kCFBooleanTrue;
        }

        if ( clean == kCFBooleanFalse )
        {
            DADiskSetState( disk, kDADiskStateRequireRepair,       TRUE );
            DADiskSetState( disk, kDADiskStateRequireRepairQuotas, TRUE );
        }

        DALogDebug( "  probed disk, id = %@, with %@, success.", disk, kind );
    }

    keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    if ( keys )
    {
        CFTypeRef object;
///w:start
        object = IORegistryEntryCreateCFProperty( DADiskGetIOMedia( disk ), CFSTR( kIOMediaWritableKey ), kCFAllocatorDefault, 0 ); 

        if ( object )
        {
            if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaWritableKey, object ) )
            {
                DADiskSetDescription( disk, kDADiskDescriptionMediaWritableKey, object );

                CFArrayAppendValue( keys, kDADiskDescriptionMediaWritableKey );
            }

            CFRelease( object );
        }
///w:stop

        object = kind ? kCFBooleanTrue : kCFBooleanFalse;

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeMountableKey, object ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeMountableKey, object );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeMountableKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeKindKey, kind ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeKindKey, kind );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeKindKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, name ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, uuid ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeUUIDKey, uuid );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeUUIDKey );
        }

        if ( CFArrayGetCount( keys ) )
        {
            if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
            {
                DADiskDescriptionChangedCallback( disk, keys );
            }
        }

        CFRelease( keys );
    }

    if ( DADiskGetState( disk, kDADiskStateStagedMount ) == FALSE )
    {
        struct statfs * mountList;
        int             mountListCount;
        int             mountListIndex;

        /*
         * Determine whether the disk is mounted.
         */

        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            if ( mountList[mountListIndex].f_fsid.val[0] == DADiskGetBSDNode( disk ) )
            {
                /*
                 * We have determined that the disk is mounted.
                 */

                CFURLRef path;

                path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault,
                                                                ( void * ) mountList[mountListIndex].f_mntonname,
                                                                strlen( mountList[mountListIndex].f_mntonname ),
                                                                TRUE );

                if ( path )
                {
                    _DAMountCreateTrashFolder( disk, path );
                    
                    DADiskSetBypath( disk, path );

                    DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, path );

                    CFRelease( path );
                }

                if ( strcmp( mountList[mountListIndex].f_mntonname, "/" ) == 0 )
                {
                    path = DAMountCreateMountPointWithAction( disk, kDAMountPointActionLink );

                    if ( path )
                    {
                        DADiskSetBypath( disk, path );

                        CFRelease( path );
                    }

                    DADiskSetOption( disk, kDADiskOptionMountAutomatic,        TRUE );
                    DADiskSetOption( disk, kDADiskOptionMountAutomaticNoDefer, TRUE );
                }

                DADiskSetState( disk, kDADiskStateRequireRepair,       FALSE );
                DADiskSetState( disk, kDADiskStateRequireRepairQuotas, FALSE );

                break;
            }
        }
    }

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DADiskSetContext( disk, NULL );

    DAStageSignal( );

    CFRelease( disk );
}

static void __DAStageRepair( DADiskRef disk )
{
    /*
     * We commence the "repair" stage if the conditions are right.
     */

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( DAUnitGetState( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        /*
         * Commence the repair.
         */

        DADiskSetState( disk, kDADiskStateStagedRepair, TRUE );

        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
        {
            CFRetain( disk );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

            DALogDebug( "  repaired disk, id = %@, ongoing.", disk );

            DAFileSystemRepair( DADiskGetFileSystem( disk ), DADiskGetDevice( disk ), __DAStageRepairCallback, disk );
        }
        else
        {
            DAStageSignal( );
        }
    }
}

static void __DAStageRepairCallback( int status, void * context )
{
    DADiskRef disk = context;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( status )
    {
        /*
         * We were unable to repair the volume.
         */

        DALogDebug( "  repaired disk, id = %@, failure.", disk );

        DALogError( "unable to repair %@ (status code 0x%08X).", disk, status );
    }
    else
    {
        /*
         * We were able to repair the volume.
         */

        DADiskSetState( disk, kDADiskStateRequireRepair, FALSE );

        DALogDebug( "  repaired disk, id = %@, success.", disk );
    }

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    /*
     * Release our resources.
     */

    CFRelease( disk );
}

CFRunLoopSourceRef DAStageCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    /*
     * Create a CFRunLoopSource for DAStage signals.
     */

    if ( __gDAStageRunLoopSource == NULL )
    {
        CFRunLoopSourceContext context = { 0 };

        context.perform = __DAStageDispatch;

        __gDAStageRunLoopSource = CFRunLoopSourceCreate( allocator, order, &context );
    }

    if ( __gDAStageRunLoopSource )
    {
        CFRetain( __gDAStageRunLoopSource );
    }

    return __gDAStageRunLoopSource;
}

void DAStageSignal( void )
{
    /*
     * Signal DAStage.
     */

    if ( gDAIdle )
    {
        ___vproc_transaction_begin( );
    }

    gDAIdle = FALSE;

    CFRunLoopSourceSignal( __gDAStageRunLoopSource );
}
