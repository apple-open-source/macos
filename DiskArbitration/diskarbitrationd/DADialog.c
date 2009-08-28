/*
 * Copyright (c) 1998-2009 Apple Inc. All Rights Reserved.
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

#include "DADialog.h"

#include "DABase.h"
#include "DAMain.h"
#include "DAQueue.h"

#include <sysexits.h>
#include <unistd.h>

static const CFStringRef __kDADialogContextDiskKey   = CFSTR( "DADialogDisk" );
static const CFStringRef __kDADialogContextSourceKey = CFSTR( "DADialogSource" );

static const CFStringRef __kDADialogTextDeviceRemoval       = CFSTR( "To eject a disk, select it in the Finder and choose File > Eject. The next time you connect the disk, Mac OS X will attempt to repair any damage to the information on the disk." );
static const CFStringRef __kDADialogTextDeviceRemovalHeader = CFSTR( "The disk was not ejected properly. If possible, always eject a disk before unplugging it or turning it off." );

static const CFStringRef __kDADialogTextDeviceUnreadableEject      = CFSTR( "Eject" );
static const CFStringRef __kDADialogTextDeviceUnreadableHeader     = CFSTR( "The disk you inserted was not readable by this computer." );
static const CFStringRef __kDADialogTextDeviceUnreadableIgnore     = CFSTR( "Ignore" );
static const CFStringRef __kDADialogTextDeviceUnreadableInitialize = CFSTR( "Initialize..." );

static const CFStringRef __kDADialogTextDeviceUnrepairable             = CFSTR( "You can still open or copy files on the disk, but you can't save changes to files on the disk. Back up the disk and reformat it as soon as you can." );
static const CFStringRef __kDADialogTextDeviceUnrepairableHeaderPrefix = CFSTR( "Mac OS X can't repair the disk \"" );
static const CFStringRef __kDADialogTextDeviceUnrepairableHeaderSuffix = CFSTR( ".\"" );

static CFMutableDictionaryRef __gDADialogList                     = NULL;
static CFRunLoopSourceRef     __gDADialogSourceDeviceRemoval      = NULL;

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
                        setgid( gDAConsoleUserGID );
                        ___initgroups( gDAConsoleUserUID, gDAConsoleUserGID );
                        setuid( gDAConsoleUserUID );

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

static void __DADialogShowDeviceUnrepairableCallback( CFUserNotificationRef notification, CFOptionFlags response )
{
    CFDictionaryRef context;

    context = CFDictionaryGetValue( __gDADialogList, notification );

    if ( context )
    {
        CFRunLoopSourceRef source;

        source = ( void * ) CFDictionaryGetValue( context, __kDADialogContextSourceKey );

        if ( source )
        {
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
            CFUserNotificationRef notification;

            CFDictionarySetValue( description, kCFUserNotificationAlertHeaderKey,     __kDADialogTextDeviceRemovalHeader );
            CFDictionarySetValue( description, kCFUserNotificationAlertMessageKey,    __kDADialogTextDeviceRemoval       );
            CFDictionarySetValue( description, kCFUserNotificationLocalizationURLKey, gDABundlePath                      );

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
            CFUserNotificationRef notification;

            CFDictionarySetValue( description, kCFUserNotificationAlertHeaderKey,        __kDADialogTextDeviceUnreadableHeader );
            CFDictionarySetValue( description, kCFUserNotificationDefaultButtonTitleKey, __kDADialogTextDeviceUnreadableEject  );
            CFDictionarySetValue( description, kCFUserNotificationLocalizationURLKey,    gDABundlePath                         );
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

            CFRelease( description );
        }

        CFRelease( context );
    }
}

void DADialogShowDeviceUnrepairable( DADiskRef disk )
{
    CFMutableDictionaryRef context;

    context = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    if ( context )
    {
        CFMutableArrayRef header;

        header = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        if ( header )
        {
            CFMutableDictionaryRef description;
            CFStringRef            name;

            name = DADiskGetDescription( disk, kDADiskDescriptionVolumeNameKey );

            if ( name == NULL )
            {
                name = CFSTR( "Untitled" );
            }

            CFArrayAppendValue( header, __kDADialogTextDeviceUnrepairableHeaderPrefix );
            CFArrayAppendValue( header, name );
            CFArrayAppendValue( header, __kDADialogTextDeviceUnrepairableHeaderSuffix );

            description = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

            if ( description )
            {
                CFUserNotificationRef notification;

                CFDictionarySetValue( description, kCFUserNotificationAlertHeaderKey,     header                            );
                CFDictionarySetValue( description, kCFUserNotificationAlertMessageKey,    __kDADialogTextDeviceUnrepairable );
                CFDictionarySetValue( description, kCFUserNotificationLocalizationURLKey, gDABundlePath                     );

                notification = CFUserNotificationCreate( kCFAllocatorDefault, 60, kCFUserNotificationStopAlertLevel, NULL, description );

                if ( notification )
                {
                    CFRunLoopSourceRef source;

                    source = CFUserNotificationCreateRunLoopSource( kCFAllocatorDefault, notification, __DADialogShowDeviceUnrepairableCallback, 0 );

                    if ( source )
                    {
                        CFDictionarySetValue( context, __kDADialogContextSourceKey, source );

                        CFDictionarySetValue( __gDADialogList, notification, context );

                        CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

                        CFRelease( source );
                    }

                    CFRelease( notification );
                }

                CFRelease( description );
            }

            CFRelease( header );
        }

        CFRelease( context );
    }
}
