/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include "DADialog.h"

#include "DAMain.h"
#include "DAQueue.h"

#include <sysexits.h>
#include <unistd.h>

static const CFStringRef __kDADialogContextDiskKey   = CFSTR( "DADialogDisk" );
static const CFStringRef __kDADialogContextSourceKey = CFSTR( "DADialogSource" );

static const CFStringRef __kDADialogPath = CFSTR( "/System/Library/PrivateFrameworks/DiskArbitration.framework" );

static const CFStringRef __kDADialogTextDeviceRemoval       = CFSTR( "The device you removed was not properly put away.  Data might have been lost or damaged.  Before you unplug your device, you must first select its icon in the Finder and choose Eject from the File menu." );
static const CFStringRef __kDADialogTextDeviceRemovalHeader = CFSTR( "Device Removal" );

static const CFStringRef __kDADialogTextDeviceUnreadable           = CFSTR( "" );
static const CFStringRef __kDADialogTextDeviceUnreadableEject      = CFSTR( "Eject" );
static const CFStringRef __kDADialogTextDeviceUnreadableHeader     = CFSTR( "You have inserted a disk containing no volumes that Mac OS X can read.  To continue with the disk inserted, click Ignore." );
static const CFStringRef __kDADialogTextDeviceUnreadableIgnore     = CFSTR( "Ignore" );
static const CFStringRef __kDADialogTextDeviceUnreadableInitialize = CFSTR( "Initialize..." );

static CFMutableDictionaryRef __gDADialogList                = NULL;
static CFRunLoopSourceRef     __gDADialogSourceDeviceRemoval = NULL;

static void __DADialogShowDeviceRemovalCallback( CFUserNotificationRef notification, CFOptionFlags response )
{
    CFRunLoopRemoveSource( CFRunLoopGetCurrent( ), __gDADialogSourceDeviceRemoval, kCFRunLoopDefaultMode );

    CFRelease( __gDADialogSourceDeviceRemoval );

    __gDADialogSourceDeviceRemoval = NULL;

    CFRelease( notification );
}

static void __DADialogShowDeviceUnreadableCallback( CFUserNotificationRef notification, CFOptionFlags response )
{
    CFDictionaryRef context;

    context = CFDictionaryGetValue( __gDADialogList, notification );

    if ( context )
    {
        CFRunLoopSourceRef source;

        source = ( void * ) CFDictionaryGetValue( context, __kDADialogContextSourceKey );

        if ( source )
        {
            DADiskRef disk;

            disk = ( void * ) CFDictionaryGetValue( context, __kDADialogContextDiskKey );

            switch ( response )
            {
                case kCFUserNotificationAlternateResponse:
                {
                    int status;

                    status = fork( );

                    if ( status == 0 )
                    {
                        setuid( gDAConsoleUserUID );
                        setgid( gDAConsoleUserGID );

                        execl( "/usr/bin/open", "/usr/bin/open", "/Applications/Utilities/Disk Utility.app", NULL );

                        exit( EX_OSERR );
                    }

                    break;
                }
                case kCFUserNotificationCancelResponse:
                case kCFUserNotificationDefaultResponse:
                {
                    if ( DADiskGetState( disk, kDADiskStateZombie ) == FALSE )
                    {
                        DADiskEject( disk, NULL );
                    }

                    break;
                }
            }

            CFRunLoopRemoveSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );
        }

        CFDictionaryRemoveValue( __gDADialogList, notification );
    }
}

void DADialogInitialize( void )
{
    __gDADialogList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
}

void DADialogShowDeviceRemoval( void )
{
    if ( __gDADialogSourceDeviceRemoval == NULL )
    {
        CFMutableDictionaryRef description;

        description = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

        if ( description )
        {
            CFURLRef path;

            path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, __kDADialogPath, kCFURLPOSIXPathStyle, TRUE );

            if ( path )
            {
                CFUserNotificationRef notification;

                CFDictionarySetValue( description, kCFUserNotificationAlertHeaderKey,     __kDADialogTextDeviceRemovalHeader );
                CFDictionarySetValue( description, kCFUserNotificationAlertMessageKey,    __kDADialogTextDeviceRemoval       );
                CFDictionarySetValue( description, kCFUserNotificationLocalizationURLKey, path                               );

                notification = CFUserNotificationCreate( kCFAllocatorDefault, 60, kCFUserNotificationStopAlertLevel, NULL, description );

                if ( notification )
                {
                    CFRunLoopSourceRef source;

                    source = CFUserNotificationCreateRunLoopSource( kCFAllocatorDefault, notification, __DADialogShowDeviceRemovalCallback, 0 );

                    if ( source )
                    {
                        CFRetain( notification );

                        CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

                        __gDADialogSourceDeviceRemoval = source;
                    }

                    CFRelease( notification );
                }

                CFRelease( path );
            }

            CFRelease( description );
        }
    }
}

void DADialogShowDeviceUnreadable( DADiskRef disk )
{
    CFMutableDictionaryRef context;

    context = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    if ( context )
    {
        CFMutableDictionaryRef description;

        description = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

        if ( description )
        {
            CFURLRef path;

            path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, __kDADialogPath, kCFURLPOSIXPathStyle, TRUE );

            if ( path )
            {
                CFUserNotificationRef notification;

                CFDictionarySetValue( description, kCFUserNotificationAlertHeaderKey,        __kDADialogTextDeviceUnreadableHeader );
                CFDictionarySetValue( description, kCFUserNotificationAlertMessageKey,       __kDADialogTextDeviceUnreadable       );
                CFDictionarySetValue( description, kCFUserNotificationDefaultButtonTitleKey, __kDADialogTextDeviceUnreadableEject  );
                CFDictionarySetValue( description, kCFUserNotificationLocalizationURLKey,    path                                  );
                CFDictionarySetValue( description, kCFUserNotificationOtherButtonTitleKey,   __kDADialogTextDeviceUnreadableIgnore );

                if ( gDAConsoleUser )
                {
                    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanTrue )
                    {
                        CFDictionarySetValue( description, kCFUserNotificationAlternateButtonTitleKey, __kDADialogTextDeviceUnreadableInitialize );
                    }
                }

                notification = CFUserNotificationCreate( kCFAllocatorDefault, 60, kCFUserNotificationStopAlertLevel, NULL, description );

                if ( notification )
                {
                    CFRunLoopSourceRef source;

                    source = CFUserNotificationCreateRunLoopSource( kCFAllocatorDefault, notification, __DADialogShowDeviceUnreadableCallback, 0 );

                    if ( source )
                    {
                        CFDictionarySetValue( context, __kDADialogContextDiskKey,   disk   );
                        CFDictionarySetValue( context, __kDADialogContextSourceKey, source );

                        CFDictionarySetValue( __gDADialogList, notification, context );

                        CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

                        CFRelease( source );
                    }

                    CFRelease( notification );
                }

                CFRelease( path );
            }

            CFRelease( description );
        }

        CFRelease( context );
    }
}
