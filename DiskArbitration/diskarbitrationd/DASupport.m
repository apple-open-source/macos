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

#include "DASupport.h"

#include "vsdb.h"
#include "DABase.h"
#include "DAFileSystem.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAThread.h"
#include "DAStage.h"
#include "DAProbe.h"
#include "DATelemetry.h"

#include <dirent.h>
#include <fsproperties.h>
#include <fstab.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/loadable_fs.h>
#include <sys/stat.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <os/feature_private.h>
#include <os/variant_private.h>

#ifdef DA_FSKIT
#include <FSKit/FSKit_private.h>
#endif
#include <FSPrivate.h>

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH || TARGET_OS_BRIDGE
#include <Security/Security.h>
#include <Security/SecTask.h>
#endif

struct __DAAuthorizeWithCallbackContext
{
    DAAuthorizeCallback callback;
    void *              callbackContext;
    DADiskRef           disk;
    _DAAuthorizeOptions options;
    char *              right;
    DASessionRef        session;
    DAReturn            status;
    gid_t               userGID;
    uid_t               userUID;
};

typedef struct __DAAuthorizeWithCallbackContext __DAAuthorizeWithCallbackContext;

static pthread_mutex_t __gDAAuthorizeWithCallbackLock = PTHREAD_MUTEX_INITIALIZER;

#ifdef DA_FSKIT
struct __FSDAModuleContext
{
    CFArrayRef               properties;
    uid_t                    user;
    void                     *probeCallbackContext;
};

struct __FSDAProbeContext {
    CFStringRef               deviceName;
    CFStringRef               bundleID;
    bool                      doFsck;
    int                       checkStatus;
    CFStringRef               volName;
    CFStringRef               volType;
    CFUUIDRef                 volUUID;
    DAFileSystemProbeCallback callback;
    void                      *callbackContext;
};

struct __FSDARepairContext {
    CFStringRef               deviceName;
    CFStringRef               bundleID;
    DAFileSystemCallback      callback;
    void                      *callbackContext;
};

typedef struct __FSDAModuleContext __FSDAModuleContext;
typedef struct __FSDAProbeContext __FSDAProbeContext;
typedef struct __FSDARepairContext __FSDARepairContext;

int __DAProbeWithFSKit( void *parameter );
int __DARepairWithFSKit( void *parameter );
int __DAFileSystemGetModulesSync( void *parameter );
void __DAFSKitProbeCallback( int status , void *parameter );
void __DAFSKitRepairCallback( int status , void *parameter );
void __DAFileSystemGetModulesCallback( int status , void *parameter );

@interface FSDATaskMessage : NSObject <FSTaskMessageOperations>
@property (retain) dispatch_group_t dispatch_group;
@property (retain) NSError *exitError;
@property BOOL didCompletion;
@end

@implementation FSDATaskMessage

- (void)logMessage:( NSString * )str
{
    DALogInfo( "%s\n" , [str UTF8String] );
}
- (void)prompt:( NSString * )prompt
  replyHandler:( void (^)( NSString * _Nullable , NSError* _Nullable ) )reply
{
    DALogInfo( "%s\n" , [prompt UTF8String] );
    reply( @"Completed prompt" , nil );
}

- (void)promptTrueFalse:( NSString * )prompt
           replyHandler:( void (^)( BOOL , NSError* _Nullable ) )reply {
    DALogInfo( "%s\n", [prompt UTF8String] );
    reply( true , nil );
}

- (void)completed:( NSError* _Nullable )error
     replyHandler:( void (^)( int ignore_me , NSError * _Nullable ) )reply
{
    DALogInfo("Completed task with error %@\n" , error );

    @synchronized ( self )
    {
        if ( _dispatch_group && !_didCompletion )
        {
            _exitError = error;
            dispatch_group_leave( _dispatch_group );
        }
    
        _didCompletion = YES;
    }

    reply( 0 , error );
}

@end

void DACheckForFSKit( void )
{
    if ([FSClient class] == NULL) {
        // No FSKit. Don't touch anything FSKit-related
        return;
    }

    gFSKitMissing = FALSE;

    return;
}
#endif /* FSKit DA functionality */

#if TARGET_OS_OSX
int __DAAuthorizeWithCallback( void * parameter )
{
    __DAAuthorizeWithCallbackContext * context = parameter;

    pthread_mutex_lock( &__gDAAuthorizeWithCallbackLock );

    context->status = DAAuthorize( context->session, context->options, context->disk, context->userUID, context->userGID,  context->right );

    pthread_mutex_unlock( &__gDAAuthorizeWithCallbackLock );

    return 0;
}

void __DAAuthorizeWithCallbackCallback( int status, void * parameter )
{
    __DAAuthorizeWithCallbackContext * context = parameter;

    ( context->callback )( context->status, context->callbackContext );

    if ( context->disk    )  CFRelease( context->disk );
    if ( context->session )  CFRelease( context->session );

    free( context->right );
    free( context );
}
#endif
DAReturn DAAuthorize( DASessionRef        session,
                      _DAAuthorizeOptions options,
                      DADiskRef           disk,
                      uid_t               userUID,
                      gid_t               userGID,
                      const char *        right )
{
    DAReturn status;

    status = kDAReturnNotPrivileged;
#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH || TARGET_OS_BRIDGE
    status = kDAReturnSuccess;
#endif

    if ( status )
    {
        if ( ( options & _kDAAuthorizeOptionIsOwner ) )
        {
            uid_t diskUID;

            diskUID = DADiskGetUserUID( disk );

            if ( diskUID == userUID )
            {
                status = kDAReturnSuccess;
            }
        }
    }
#if TARGET_OS_OSX
    if ( status )
    {
        AuthorizationRef authorization;

        authorization = DASessionGetAuthorization( session );

        if ( authorization )
        {
            AuthorizationFlags  flags;
            AuthorizationItem   item;
            char *              name;
            AuthorizationRights rights;

            flags = kAuthorizationFlagExtendRights;

            if ( ( options & _kDAAuthorizeOptionAuthenticateAdministrator ) )
            {
                flags |= kAuthorizationFlagInteractionAllowed;

                asprintf( &name, "system.volume.workgroup.%s", right );
            }
            else
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeNetworkKey ) == kCFBooleanTrue )
                {
                    asprintf( &name, "system.volume.network.%s", right );
                }
                else
                {
                    CFTypeRef object;

                    object = DADiskGetDescription( disk, kDADiskDescriptionDeviceProtocolKey );

                    if ( object && CFEqual( object, CFSTR( kIOPropertyPhysicalInterconnectTypeVirtual ) ) )
                    {
                        asprintf( &name, "system.volume.virtual.%s", right );
                    }
                    else
                    {
                        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
                        {
                            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
                            {
                                asprintf( &name, "system.volume.optical.%s", right );
                            }
                            else
                            {
                                asprintf( &name, "system.volume.removable.%s", right );
                            }
                        }
                        else
                        {
                            if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                            {
                                asprintf( &name, "system.volume.internal.%s", right );
                            }
                            else
                            {
                                asprintf( &name, "system.volume.external.%s", right );
                            }
                        }
                    }
                }
            }

            if ( name )
            {
                item.flags       = 0;
                item.name        = name;
                item.value       = NULL;
                item.valueLength = 0;

                rights.count = 1;
                rights.items = &item;

                status = AuthorizationCopyRights( authorization, &rights, NULL, flags, NULL );

                if ( status )
                {
                    status = kDAReturnNotPrivileged;
                }

                free( name );
            }
        }
    }
#endif
    return status;
}

#if TARGET_OS_OSX
void DAAuthorizeWithCallback( DASessionRef        session,
                              _DAAuthorizeOptions options,
                              DADiskRef           disk,
                              uid_t               userUID,
                              gid_t               userGID,
                              DAAuthorizeCallback callback,
                              void *              callbackContext,
                              const char *        right )
{
    __DAAuthorizeWithCallbackContext * context;

    context = malloc( sizeof( __DAAuthorizeWithCallbackContext ) );

    if ( context )
    {
        if ( disk    )  CFRetain( disk );
        if ( session )  CFRetain( session );

        context->callback        = callback;
        context->callbackContext = callbackContext;
        context->disk            = disk;
        context->options         = options;
        context->right           = strdup( right );
        context->session         = session;
        context->status          = kDAReturnNotPrivileged;
        context->userGID         = userGID;
        context->userUID         = userUID;

        DAThreadExecute( __DAAuthorizeWithCallback, context, __DAAuthorizeWithCallbackCallback, context );
    }
    else
    {
        ( callback )( kDAReturnNotPrivileged, callbackContext );
    }
}
#endif

static struct timespec __gDAFileSystemListTime1 = { 0, 0 };
static struct timespec __gDAFileSystemListTime2 = { 0, 0 };

const CFStringRef kDAFileSystemKey = CFSTR( "DAFileSystem" );

static void __DAFileSystemProbeListAppendValue( const void * key, const void * value, void * context )
{
    CFMutableDictionaryRef probe;

    probe = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, value );

    if ( probe )
    {
        CFDictionarySetValue( probe, kDAFileSystemKey, context );
        CFArrayAppendValue( gDAFileSystemProbeList, probe );
        CFRelease( probe );
    }
}

static CFComparisonResult __DAFileSystemProbeListCompare( const void * value1, const void * value2, void * context )
{
    CFNumberRef order1 = CFDictionaryGetValue( value1, CFSTR( kFSProbeOrderKey ) );
    CFNumberRef order2 = CFDictionaryGetValue( value2, CFSTR( kFSProbeOrderKey ) );

    if ( order1 == NULL )  return kCFCompareGreaterThan;
    if ( order2 == NULL )  return kCFCompareLessThan;

    return CFNumberCompare( order1, order2, NULL );
}

static void __DAFileSystemListRefresh( const char * directory )
{
    CFURLRef base;

    base = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) directory, strlen( directory ), TRUE );

    if ( base )
    {
        DIR * folder;

        /*
         * Scan the filesystems in the file system folder.
         */

        folder = opendir( directory );

        if ( folder )
        {
            struct dirent * item;

            DALogDebugHeader( "filesystems have been refreshed." );

            while ( ( item = readdir( folder ) ) )
            {
                char * suffix;

                suffix = item->d_name + strlen( item->d_name ) - strlen( FS_DIR_SUFFIX );

                if ( suffix > item->d_name )
                {
                    if ( strcmp( suffix, FS_DIR_SUFFIX ) == 0 )
                    {
#ifdef DA_FSKIT
                        if ( !gFSKitMissing
                             && os_feature_enabled(FSKit, msdosUseFSKitModule) && strcmp(item->d_name, "msdos.fs") == 0)
                        {
                            DALogInfo( "Skipping msdos.fs as msdosUseFSKitModule pref is on");
                            continue;
                        }
#endif 

                        
                        CFURLRef path;

                        path = CFURLCreateFromFileSystemRepresentationRelativeToBase( kCFAllocatorDefault,
                                                                                      ( void * ) item->d_name,
                                                                                      strlen( item->d_name ),
                                                                                      TRUE,
                                                                                      base );

                        
                        if ( path )
                        {
                            DAFileSystemRef filesystem;

                            /*
                             * Create a file system object for this file system.
                             */

                            filesystem = DAFileSystemCreate( kCFAllocatorDefault, path );

                            if ( filesystem )
                            {
                                CFDictionaryRef probe;

                                /*
                                 * Add this file system object to our list.
                                 */

                                DALogDebug( "  created filesystem, id = %@.", filesystem );

                                CFArrayAppendValue( gDAFileSystemList, filesystem );

                                probe = DAFileSystemGetProbeList( filesystem );

                                if ( probe )
                                {
                                    CFDictionaryApplyFunction( probe, __DAFileSystemProbeListAppendValue, filesystem );
                                }

                                CFRelease( filesystem );
                            }

                            CFRelease( path );
                        }
                    }
                }
            }

            closedir( folder );
        }

        CFRelease( base );
    }
}

#ifdef DA_FSKIT
CFStringRef DSFSKitGetBundleNameWithoutSuffix( CFStringRef filesystemName )
{
    CFStringRef                  fsname            = NULL;
    CFCharacterSetRef            cset;
    CFRange                      range;
    
    /* Remove the '_fskit' from the fs name */
    cset = CFCharacterSetCreateWithCharactersInString( kCFAllocatorDefault , CFSTR("_") );
    
    CFStringFindCharacterFromSet( filesystemName , cset , CFRangeMake( 0 , CFStringGetLength( filesystemName ) ),
                                 0 , &range );
    
    fsname = CFStringCreateWithSubstring( kCFAllocatorDefault , filesystemName ,
                                         CFRangeMake( 0 , range.location ) );
    CFRelease( cset );
    return fsname;
}

/*
 * Given a bundle name in the form 'fsname_fskit', convert it to a bundle ID in the form 'com.apple.fskit.fsname'.
 */
CFStringRef DAGetFSKitBundleID( CFStringRef filesystemName )
{
    CFStringRef                  bundleID          = NULL;
    CFStringRef                  fsName            = NULL;
    
    fsName = DSFSKitGetBundleNameWithoutSuffix( filesystemName );
    
    bundleID = CFStringCreateWithFormat( kCFAllocatorDefault ,
                                         NULL , CFSTR("com.apple.fskit.%@") , fsName );
    CFRelease (fsName);
    
    return bundleID;
}

static NSDictionary *__propertiesForFSModule( FSModuleIdentity *fsmodule ) {
    NSDictionary *extAttributes = fsmodule.attributes, *mediaTypes, *personalities;
    NSMutableDictionary *module = [[NSMutableDictionary alloc] init];
    NSString *fsname = [NSString stringWithFormat:@"%@_fskit", extAttributes[@"FSShortName"]];
    NSNumber *supportsBlockResources = extAttributes[@"FSSupportsBlockResources"];
    
    if (fsname && module && [supportsBlockResources boolValue]) {
        [module setValue:fsname forKey:(NSString *) kCFBundleNameKey];
        [module setValue:[NSNumber numberWithBool:true] forKey:@"FSIsFSModule"];
        [module setValue:@[@"UserFS", @"kext"] forKey:@"FSImplementation"];

        mediaTypes = extAttributes[@kFSMediaTypesKey];
        personalities = extAttributes[@kFSPersonalitiesKey];

        if (mediaTypes && personalities) {
            [module setValue:mediaTypes forKey:@kFSMediaTypesKey];
            [module setValue:personalities forKey:@kFSPersonalitiesKey];
            DALog("Found FSModule: %@", module);
        } else {
            DALogError("FSModule missing information");
            module = nil;
        }
    }
    
    return module;
}

/* Similar to __DAProbeCallback but only for checking FSKit probes */
static void __FSKitProbeStatusCallback( int status ,
                                        int cleanStatus ,
                                        CFStringRef name ,
                                        CFStringRef type ,
                                        CFUUIDRef uuid ,
                                        void * parameter )
{
    /*
     * Process the probe command's completion.
     */
    __DAProbeCallbackContext * context = parameter;
    bool doFsck                        = true;
    bool didProbe                      = false;
    char *containerBSDPath             = NULL;
    DADiskRef disk                     = context->disk;
    CFURLRef device                    = DADiskGetDevice( disk );
    CFStringRef deviceName             = CFURLCopyLastPathComponent( device );
    CFStringRef bundleID;
    
#if !TARGET_OS_OSX
    if ( ( ( DADiskGetDescription( disk , kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue ) &&
          ( DADiskGetDescription( disk , kDADiskDescriptionDeviceInternalKey ) == NULL ) ) ||
        ( DADiskGetDescription( disk , kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue ) )
    {
        doFsck = false;
    }
#endif
    
    /*
     * We have found no probe match for this media object.
     */
    if ( status )
    {
        /* We are returning from a failed probe with a filesystem previously set */
        if ( context->filesystem )
        {
            CFStringRef kind = DAFileSystemGetKind( context->filesystem );
            didProbe = true;
            DALogInfo( "probed disk, id = %@, with %@, failure." , context->disk , kind );
            
            if ( status != FSUR_UNRECOGNIZED )
            {
                DALogError( "unable to probe %@ (status code 0x%08X)." , context->disk , status );
            }
            
            CFRelease( context->filesystem );
            context->filesystem = NULL;
        }
        
        /* Get the next matching probe candidate while removing the others from the list */
        while ( CFArrayGetCount( context->candidates ) > 0 )
        {
            CFDictionaryRef probe = CFArrayGetValueAtIndex( context->candidates , 0 );
            DAFileSystemRef filesystem = ( void * ) CFDictionaryGetValue( probe , kDAFileSystemKey );
            
            if ( filesystem )
            {
                CFDictionaryRef properties = CFDictionaryGetValue( probe , CFSTR( kFSMediaPropertiesKey ) );
                CFStringRef kind = DAFileSystemGetKind( filesystem );
                CFStringRef bundleID;
                
                CFRetain( filesystem );
                context->filesystem = filesystem;
                
                if ( properties )
                {
                    boolean_t match = FALSE;
                    IOServiceMatchPropertyTable( DADiskGetIOMedia( disk ) , properties , &match );
                    
                    if ( match )
                    {
                        /*
                         * We have found a probe candidate for this media object.
                         */
                        if ( CFDictionaryGetValue( probe , CFSTR( "autodiskmount" ) ) == kCFBooleanFalse )
                        {
                            DADiskSetState( disk, _kDADiskStateMountAutomatic,        FALSE );
                            DADiskSetState( disk, _kDADiskStateMountAutomaticNoDefer, FALSE );
                        }
                        
                        CFArrayRemoveValueAtIndex( context->candidates , 0 );
                        DALogInfo( "probed disk, id = %@, with %@, ongoing.", disk , kind );
                        
                        bundleID = DAGetFSKitBundleID( DAFileSystemGetKind( filesystem ) );
                        
                        DAProbeWithFSKit( deviceName ,
                                          bundleID ,
                                          doFsck ,
                                          __FSKitProbeStatusCallback ,
                                          context );
                        return; /* Do not over-release any context */
                    }
                }
            }
            CFArrayRemoveValueAtIndex( context->candidates , 0 ); /* clear out non-matching candidates */
        }
    }
    else
    {
        /*
         * We have found a probe match for this media object via FSKit.
         */
        CFStringRef kind = DAFileSystemGetKind( context->filesystem );
        didProbe = true;
        DALogInfo( "probed disk, id = %@, with %@, success.", context->disk, kind );
    }
    
    if ( context->callback )
    {
        if ( context->filesystem && didProbe )
        {
            DATelemetrySendProbeEvent( status , DAFileSystemGetKind( context->filesystem ) , CFSTR("FSKit") , 
                clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - context->startTime , cleanStatus );
        }
        ( context->callback )( status ,
                               context->filesystem ,
                               cleanStatus ,
                               name ,
                               type ,
                               uuid ,
                               context->callbackContext );
    }
    
    if ( context->candidates )
    {
        CFRelease( context->candidates );
    }
    
    if ( context->disk )
    {
        CFRelease( context->disk       );
    }
    
    if ( context->filesystem )
    {
        CFRelease( context->filesystem );
    }

    free( context );
}

static void __DAFileSystemListRefreshModules( uid_t user ,
                                              void *probeCallbackContext )
{
    __FSDAModuleContext *context = malloc( sizeof( __FSDAModuleContext ) );
    
    if ( context == NULL )
    {
        return;
    }
    
    context->properties = NULL;
    context->user = user;
    context->probeCallbackContext = probeCallbackContext;
    
    DAThreadExecute( __DAFileSystemGetModulesSync , context , __DAFileSystemGetModulesCallback , context );
}

void __DAFileSystemGetModulesCallback( int status , void *parameter )
{
    __FSDAModuleContext *context = parameter;
    NSArray<NSDictionary *> *properties = ( __bridge NSArray<NSDictionary *> * ) context->properties;
    struct __DAProbeCallbackContext *probeCallbackContext = context->probeCallbackContext;
    CFIndex        count;
    CFIndex        index;
    
    /* Add filesystem probes to our probe context's list of candidates */
    probeCallbackContext->candidates = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
    
    [properties enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        DAFileSystemRef filesystem;
        
        /*
         * Create a file system object for this file system.
         */
        filesystem = DAFileSystemCreateFromProperties( kCFAllocatorDefault, (__bridge CFDictionaryRef) obj );
        
        if ( filesystem )
        {
            DADiskRef disk = probeCallbackContext->disk;
            CFDictionaryRef probes;
            
            DALogDebug( " created filesystem, id = %@." , filesystem );

            /*
             * Process each probe. Check if it matches and call probeWithFSKit()
             */
            probes = DAFileSystemGetProbeList( filesystem );
            if ( probes )
            {
                int numProbes = CFDictionaryGetCount( probes );
                CFDictionaryRef *probeArray = (CFDictionaryRef *)malloc( sizeof( CFDictionaryRef ) * numProbes );
                CFDictionaryGetKeysAndValues( probes , NULL, (const void **) probeArray );
                
                for (int i = 0; i < numProbes; i++)
                {
                    CFDictionaryRef probe = probeArray[i];
                    CFMutableDictionaryRef probeCopy = CFDictionaryCreateMutableCopy( kCFAllocatorDefault,
                                                                                     0,
                                                                                     probe );
                    
                    if ( probeCopy )
                    {
                        CFDictionarySetValue( probeCopy, kDAFileSystemKey, filesystem );
                        CFArrayAppendValue( probeCallbackContext->candidates , probeCopy );
                        CFRelease( probeCopy );
                    }
                }
                free( probeArray );
            }
            
            CFRelease( filesystem );
        }
    }];
    
    /* Kick off the first probe and let the callback process the rest of the probes */
    __FSKitProbeStatusCallback( -1 , -1 , NULL , NULL , NULL , probeCallbackContext );
    
    free( context );
}

int __DAFileSystemGetModulesSync( void *parameter )
{
    __FSDAModuleContext *context = parameter;
    FSClient *client = [FSClient new];
    FSAuditToken *token = nil;
    __block NSMutableArray<NSDictionary *> *properties = [NSMutableArray new];
    dispatch_group_t group = dispatch_group_create();
    CFMutableArrayRef result = NULL;
    uid_t user = context->user;

/* We don't use the configuration callback on iOS, and diskarbitrationd runs as root on iOS, so default to 501 */
#if TARGET_OS_IOS
    if ( user == ___UID_ROOT )
    {
        user = 501;
    }
#endif
    
    token = [FSAuditToken new];
    
    if ( user != 0 ) {
        token = [token tokenWithRuid:user];
    }
    
    dispatch_group_enter(group);
    
    /* Set up the properties the same way +[FSModuleHost installedExtensionPropertiesSync] does in FSKit */
    if ( token && client ) {
        [client installedExtensionsForUser:token.audit_token replyHandler:^(NSArray<FSModuleIdentity *> * _Nullable fsmodules,
                                                                            NSError * _Nullable err) {
            if (!err) {
                [fsmodules enumerateObjectsUsingBlock:^(FSModuleIdentity * _Nonnull obj, NSUInteger idx,
                                                        BOOL * _Nonnull stop) {
                    NSDictionary *module = __propertiesForFSModule( obj );
                    
                    if ( module ) {
                        [properties addObject:module];
                    }
                }];
            } else {
                DALogError("Unable to retrieve FSModules for uid %u: %@", user, err);
            }
            dispatch_group_leave( group );
        }];
    }
    else
    {
        DALogError("Unable to retrieve FSModules for uid %u: infrastructure issues", user);
        dispatch_group_leave( group );
    }
    
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    
    context->properties = CFRetain( ( __bridge CFArrayRef ) properties );
    
    return 0;
}

/* Retrieve the FSModule probe candidates and associated filesystems for the specified user */
void DAGetFSModulesForUser( uid_t user ,
                            void *probeCallbackContext )
{
    __DAFileSystemListRefreshModules( user , probeCallbackContext );
}

void DAProbeWithFSKit( CFStringRef deviceName ,
                       CFStringRef bundleID ,
                       bool doFsck ,
                       DAFileSystemProbeCallback callback ,
                       void *callbackContext )
{
    __FSDAProbeContext *context;

    if ( gFSKitMissing )
    {
        callback( ENOENT , ENOENT , NULL , NULL , NULL , callbackContext );
        CFRelease( bundleID );
        CFRelease( deviceName );
        return;
    }

    context = malloc( sizeof( __FSDAProbeContext ) );
    if ( context == NULL )
    {
        callback( ENOMEM , ENOMEM , NULL , NULL , NULL , callbackContext );
        CFRelease( bundleID );
        CFRelease( deviceName );
        return;
    }
    
    context->deviceName = deviceName;
    context->bundleID = bundleID;
    context->doFsck = doFsck;
    context->volName = NULL;
    context->volType = NULL;
    context->volUUID = NULL;
    context->checkStatus = 0;
    context->callback = callback;
    context->callbackContext = callbackContext;
    
    DAThreadExecute( __DAProbeWithFSKit , context , __DAFSKitProbeCallback , context );
}
    
int __DAProbeWithFSKit( void *parameter )
{
    FSClient                  *client;
    FSBlockDeviceResource     *res;
    __FSDAProbeContext        *context         = parameter;
    CFStringRef               deviceName       = context->deviceName;
    CFStringRef               bundleID         = context->bundleID;
    bool                      doFsck           = context->doFsck;
    DAFileSystemProbeCallback callback         = context->callback;
    void                      *callbackContext = context->callbackContext;
    FSAuditToken              *token;
    __block int               status           = 0;
    dispatch_group_t          probeGroup       = dispatch_group_create();
    FSTaskOptionsBundle       *options;
    FSMessageReceiver         *msgRcvr;
    FSMessageConnection       *connection;
    FSTaskOptionsBundle       *taskOptions;
    FSDATaskMessage           *messageDumper;
    dispatch_group_t          checkGroup       = dispatch_group_create();
    __block NSUUID            *checkTaskID;
    uid_t                     user             = gDAConsoleUserUID;

/* We don't use the configuration callback on iOS, and diskarbitrationd runs as root, so default to 501 */
#if TARGET_OS_IOS
    user = 501;
#endif
    client = [FSClient new];
    res = [FSBlockDeviceResource proxyResourceForBSDName:(__bridge NSString *) deviceName];
    token = [FSAuditToken new];
    token = [token tokenWithRuid:user];
    
    dispatch_group_enter( probeGroup );
    [client probeResource:res
              usingBundle: (__bridge NSString *) bundleID
               auditToken:token.audit_token
             replyHandler:^(FSProbeResult * _Nullable result,
                                NSError * _Nullable probeErr) {
        NSString                *trimmedName = nil;
        __block CFStringRef     volumeType = NULL;
        
        if ( probeErr )
        {
            status = (int) [probeErr code];
        }
        else if ( result )
        {
            switch ( result.result )
            {
                case FSMatchResultUsable:
                    status = 0;
                    break;
                case FSMatchResultNotRecognized:
                    status = ENOENT;
                    break;
                default:
                    status = EIO;
            }
        } else {
            status = EIO;
        }
        if ( status )
        {
            context->checkStatus = status;
            dispatch_group_leave( probeGroup );
            return;
        }
        
        volumeType =
        _FSCopyNameForVolumeFormatAtNode( ( __bridge CFStringRef ) res.devicePath );
        if ( volumeType )
        {
            context->volType = volumeType;
        }
        
        if ( result.name )
        {
            /* Handle empty name */
            if ( ![result.name hasPrefix:@"\0"] )
            {
                trimmedName = [result.name stringByTrimmingCharactersInSet:
                               [NSCharacterSet characterSetWithCharactersInString:@"\0"]];
            }
        }
        
        if ( trimmedName && [trimmedName length] )
        {
            context->volName = CFRetain( ( __bridge CFStringRef ) trimmedName );
        }
        else
        {
            if ( [(__bridge NSString *) bundleID hasSuffix:@"msdos"] )
            {
                context->volName = CFRetain(CFSTR("NO NAME"));
            }
            else
            {
                context->volName = CFRetain(CFSTR("Untitled"));
            }
        }
        
        if ( result.containerID )
        {
            // In the future, this needs work. Container IDs CAN be more than a UUID. May sling this around as a string...
            context->volUUID = CFUUIDCreateFromString( kCFAllocatorDefault ,
                                                      ( __bridge CFStringRef ) result.containerID.uuid.description );
        }
        
        dispatch_group_leave( probeGroup );
    }];
    dispatch_group_wait( probeGroup , DISPATCH_TIME_FOREVER );
    
    if ( ( status == 0 ) && context->doFsck )
    {
        // Pass the connection to FSClient
        messageDumper   = [FSDATaskMessage new];
        msgRcvr         = [FSMessageReceiver receiverWithDelegate:messageDumper];
        connection      = [msgRcvr getConnection];
        options         = [FSTaskOptionsBundle new];
        
        [options addOption:[FSTaskOption optionWithoutValue:@"q"]];

        dispatch_group_enter( checkGroup );
        messageDumper.dispatch_group = checkGroup;
        dispatch_group_enter( checkGroup ); // also wait for the reply handler to exit
        
        [client checkResource:res
                  usingBundle:(__bridge NSString *) bundleID
                      options:options
                   auditToken:token.audit_token
                   connection:connection
                 replyHandler:^(NSUUID * _Nullable taskID, NSError * _Nullable checkErr) {
            context->checkStatus = checkErr ? (int) [checkErr code] : 0;
            DALogInfo("check resource handler called with error %@", checkErr);
            if ( checkErr )
            {
                [messageDumper completed:checkErr replyHandler:^(int ignore_me, NSError * _Nullable innerErr) {
                    // nothing to do here - messageDumper will call dispatch_group_leave() in completion handler
                }];
            } else
            {
                checkTaskID = taskID;
            }
            dispatch_group_leave( checkGroup ); // make sure the reply handler returns first
        }];
        dispatch_group_wait( checkGroup , DISPATCH_TIME_FOREVER );
        
        if ( messageDumper.exitError ) {
            context->checkStatus = (int)messageDumper.exitError.code; // Not quite right, but a start
        }
        DALogInfo("FSKit check of resource %@ exited with error %@ %d", res, messageDumper.exitError, context->checkStatus );
    }
    
    return status;
}

void __DAFSKitProbeCallback( int status , void *parameter )
{
    __FSDAProbeContext *context = parameter;
    
    if ( context->callback ) {
        context->callback( status ,
                           context->checkStatus ,
                           context->volName ,
                           context->volType ,
                           context->volUUID ,
                           context->callbackContext );
    }
    
    CFRelease( context->bundleID );
    CFRelease( context->deviceName );
    
    if ( context->volName )
    {
        CFRelease( context->volName );
    }
    
    if ( context->volType )
    {
        CFRelease( context->volType );
    }
    
    if ( context->volUUID )
    {
        CFRelease( context->volUUID );
    }
    
    free( parameter );
}

void DARepairWithFSKit( CFStringRef deviceName ,
                        CFStringRef bundleID ,
                        DAFileSystemCallback callback ,
                        void *callbackContext )
{
    __FSDARepairContext *context = malloc( sizeof( __FSDARepairContext ) );
    
    if ( context == NULL )
    {
        callback( ENOMEM , callbackContext );
        CFRelease( bundleID );
        CFRelease( deviceName );
        return;
    }
    
    context->deviceName = deviceName;
    context->bundleID = bundleID;
    context->callback = callback;
    context->callbackContext = callbackContext;
    
    DAThreadExecute( __DARepairWithFSKit , context , __DAFSKitRepairCallback , context );
}

void __DAFSKitRepairCallback( int status , void *parameter )
{
    __FSDARepairContext *context = parameter;
    
    if ( context->callback ) {
        context->callback( status ,
                           context->callbackContext );
    }
    
    CFRelease( context->bundleID );
    CFRelease( context->deviceName );
    
    free( parameter );
}

int __DARepairWithFSKit( void *parameter )
{
    FSClient *client;
    FSBlockDeviceResource *res;
    __FSDARepairContext     *context         = parameter;
    CFStringRef             deviceName       = context->deviceName;
    CFStringRef             bundleID         = context->bundleID;
    DAFileSystemCallback    callback         = context->callback;
    void                    *callbackContext = context->callbackContext;
    FSTaskOptionsBundle     *options;
    FSMessageReceiver       *msgRcvr;
    FSMessageConnection     *connection;
    FSTaskOptionsBundle     *taskOptions;
    FSDATaskMessage         *messageDumper;
    FSAuditToken            *token;
    __block int             status           = 0;
    dispatch_group_t        group            = dispatch_group_create();
    __block NSUUID         *checkTaskID;
    uid_t                   user             = gDAConsoleUserUID;

/* We don't use the configuration callback on iOS, and diskarbitrationd runs as root, so default to 501 */
#if TARGET_OS_IOS
    user = 501;
#endif
    
    client = [FSClient new];
    res = [FSBlockDeviceResource proxyResourceForBSDName:(__bridge NSString *) deviceName
                                                writable:YES];
    
    // Pass the connection to FSClient
    messageDumper   = [FSDATaskMessage new];
    msgRcvr         = [FSMessageReceiver receiverWithDelegate:messageDumper];
    connection      = [msgRcvr getConnection];
    options         = [FSTaskOptionsBundle new];
    
    [options addOption:[FSTaskOption optionWithoutValue:@"y"]];

    token = [FSAuditToken new];
    token = [token tokenWithRuid:user];
    
    dispatch_group_enter( group );
    messageDumper.dispatch_group = group;
    dispatch_group_enter( group ); // also wait for the reply handler to exit

    [client checkResource:res
              usingBundle:(__bridge NSString *) bundleID
                  options:options
               auditToken:token.audit_token
               connection:connection
             replyHandler:^(NSUUID * _Nullable taskID, NSError * _Nullable checkErr) {
        DALogInfo("check resource handler called with error %@", checkErr);
        if ( checkErr )
        {
            status = (int) [checkErr code];
            [messageDumper completed:checkErr replyHandler:^(int ignore_me, NSError * _Nullable innerErr) {
                // nothing to do here - messageDumper will call dispatch_group_leave() in completion handler
            }];
        } else
        {
            checkTaskID = taskID;
        }
        dispatch_group_leave( group ); // make sure the reply handler returns first
    }];
    
    dispatch_group_wait( group , DISPATCH_TIME_FOREVER );
    DALogInfo("FSKit check of resource %@ exited with error %@", res, messageDumper.exitError);
    if ( status == 0 && messageDumper.exitError ) // operation completed with error outside error handler
    {
        status = (int) messageDumper.exitError.code;
    }
    
    return status;
}

#endif /* DA_FSKIT */

void DAFileSystemListRefresh( void )
{
    struct stat status1;
    struct stat status2;

    /*
     * Determine whether the file system list is up-to-date.
     */

    if ( stat( FS_DIR_LOCATION, &status1 ) )
    {
        __gDAFileSystemListTime1.tv_sec  = 0;
        __gDAFileSystemListTime1.tv_nsec = 0;
    }

    if ( stat( ___FS_DEFAULT_DIR, &status2 ) )
    {
        __gDAFileSystemListTime2.tv_sec  = 0;
        __gDAFileSystemListTime2.tv_nsec = 0;
    }

    if ( __gDAFileSystemListTime1.tv_sec  != status1.st_mtimespec.tv_sec  ||
         __gDAFileSystemListTime1.tv_nsec != status1.st_mtimespec.tv_nsec ||
         __gDAFileSystemListTime2.tv_sec  != status2.st_mtimespec.tv_sec  ||
         __gDAFileSystemListTime2.tv_nsec != status2.st_mtimespec.tv_nsec )
    {
        __gDAFileSystemListTime1.tv_sec  = status1.st_mtimespec.tv_sec;
        __gDAFileSystemListTime1.tv_nsec = status1.st_mtimespec.tv_nsec;
        __gDAFileSystemListTime2.tv_sec  = status2.st_mtimespec.tv_sec;
        __gDAFileSystemListTime2.tv_nsec = status2.st_mtimespec.tv_nsec;

        /*
         * Clear the file system list.
         */

        CFArrayRemoveAllValues( gDAFileSystemList );
        CFArrayRemoveAllValues( gDAFileSystemProbeList );

        /*
         * Build the file system list.
         */

        __DAFileSystemListRefresh( FS_DIR_LOCATION );
        __DAFileSystemListRefresh( ___FS_DEFAULT_DIR );

        /*
         * Order the probe list.
         */

        CFArraySortValues( gDAFileSystemProbeList,
                           CFRangeMake( 0, CFArrayGetCount( gDAFileSystemProbeList ) ),
                           __DAFileSystemProbeListCompare,
                           NULL );
    }
}

static struct timespec __gDAMountMapListTime1 = { 0, 0 };
static struct timespec __gDAMountMapListTime2 = { 0, 0 };

const CFStringRef kDAMountMapMountAutomaticKey = CFSTR( "DAMountAutomatic" );
const CFStringRef kDAMountMapMountOptionsKey   = CFSTR( "DAMountOptions"   );
const CFStringRef kDAMountMapMountPathKey      = CFSTR( "DAMountPath"      );
const CFStringRef kDAMountMapProbeIDKey        = CFSTR( "DAProbeID"        );
const CFStringRef kDAMountMapProbeKindKey      = CFSTR( "DAProbeKind"      );

static CFDictionaryRef __DAMountMapCreate1( CFAllocatorRef allocator, struct fstab * fs )
{
    CFMutableDictionaryRef map = NULL;

    if ( strcmp( fs->fs_type, FSTAB_SW ) )
    {
        char * idAsCString = fs->fs_spec;

        strsep( &idAsCString, "=" );

        if ( idAsCString )
        {
            CFStringRef idAsString;

            idAsString = CFStringCreateWithCString( kCFAllocatorDefault, idAsCString, kCFStringEncodingUTF8 );

            if ( idAsString )
            {
                CFTypeRef id = NULL;

                if ( strcmp( fs->fs_spec, "UUID" ) == 0 )
                {
                    id = ___CFUUIDCreateFromString( kCFAllocatorDefault, idAsString );
                }
                else if ( strcmp( fs->fs_spec, "LABEL" ) == 0 )
                {
                    id = CFRetain( idAsString );
                }
                else if ( strcmp( fs->fs_spec, "DEVICE" ) == 0 )
                {
                    id = ___CFDictionaryCreateFromXMLString( kCFAllocatorDefault, idAsString );
                }

                if ( id )
                {
                    map = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                    if ( map )
                    {
                        CFMutableStringRef options;

                        options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

                        if ( options )
                        {
                            char *       argument  = NULL;
                            char *       arguments = fs->fs_mntops;
                            CFBooleanRef automatic = NULL;

                            while ( ( argument = strsep( &arguments, "," ) ) )
                            {
                                if ( strcmp( argument, "auto" ) == 0 )
                                {
                                    automatic = kCFBooleanTrue;
                                }
                                else if ( strcmp( argument, "noauto" ) == 0 )
                                {
                                    automatic = kCFBooleanFalse;
                                }
                                else
                                {
                                    CFStringAppendCString( options, argument, kCFStringEncodingUTF8 );    
                                    CFStringAppendCString( options, ",", kCFStringEncodingUTF8 );
                                }
                            }

                            if ( automatic )
                            {
                                CFDictionarySetValue( map, kDAMountMapMountAutomaticKey, automatic );
                            }

                            if ( CFStringGetLength( options ) )
                            {
                                CFStringTrim( options, CFSTR( "," ) );

                                CFDictionarySetValue( map, kDAMountMapMountOptionsKey, options );
                            }

                            CFRelease( options );
                        }

                        if ( strcmp( fs->fs_file, "none" ) )
                        {
                            CFURLRef path;

                            path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) fs->fs_file, strlen( fs->fs_file ), TRUE );

                            if ( path )
                            {
                                CFDictionarySetValue( map, kDAMountMapMountPathKey, path );

                                CFRelease( path );
                            }
                        }

                        if ( strcmp( fs->fs_vfstype, "auto" ) )
                        {
                            CFStringRef kind;

                            kind = CFStringCreateWithCString( kCFAllocatorDefault, fs->fs_vfstype, kCFStringEncodingUTF8 );

                            if ( kind )
                            {
                                CFDictionarySetValue( map, kDAMountMapProbeKindKey, kind );

                                CFRelease( kind );
                            }
                        }

                        CFDictionarySetValue( map, kDAMountMapProbeIDKey, id );
                    }

                    CFRelease( id );
                }

                CFRelease( idAsString );
            }
        }
    }

    return map;
}

void DAMountMapListRefresh1( void )
{
    struct stat status;

    /*
     * Determine whether the mount map list is up-to-date.
     */

    if ( stat( _PATH_FSTAB, &status ) )
    {
        __gDAMountMapListTime1.tv_sec  = 0;
        __gDAMountMapListTime1.tv_nsec = 0;
    }

    if ( __gDAMountMapListTime1.tv_sec  != status.st_mtimespec.tv_sec  ||
         __gDAMountMapListTime1.tv_nsec != status.st_mtimespec.tv_nsec )
    {
        __gDAMountMapListTime1.tv_sec  = status.st_mtimespec.tv_sec;
        __gDAMountMapListTime1.tv_nsec = status.st_mtimespec.tv_nsec;

        /*
         * Clear the mount map list.
         */

        CFArrayRemoveAllValues( gDAMountMapList1 );

        /*
         * Build the mount map list.
         */

        if ( setfsent( ) )
        {
            struct fstab * item;

            while ( ( item = getfsent( ) ) )
            {
                CFDictionaryRef map;

                map = __DAMountMapCreate1( kCFAllocatorDefault, item );

                if ( map )
                {
                    CFArrayAppendValue( gDAMountMapList1, map );

                    CFRelease( map );
                }
            }

            endfsent( );
        }
    }
}

static CFDictionaryRef __DAMountMapCreate2( CFAllocatorRef allocator, struct vsdb * vs )
{
    CFStringRef            idAsString;
    CFMutableDictionaryRef map = NULL;

    idAsString = CFStringCreateWithCString( kCFAllocatorDefault, vs->vs_spec, kCFStringEncodingUTF8 );

    if ( idAsString )
    {
        CFTypeRef id;

        id = _DAFileSystemCreateUUIDFromString( kCFAllocatorDefault, idAsString );

        if ( id )
        {
            map = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

            if ( map )
            {
                CFMutableStringRef options;

                options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

                if ( options )
                {
                    if ( ( vs->vs_ops & VSDB_PERM ) )
                    {
                        CFStringAppend( options, CFSTR( "owners" ) );
                        CFStringAppend( options, CFSTR( "," ) );
                    }
                    else
                    {
                        CFStringAppend( options, CFSTR( "noowners" ) );
                        CFStringAppend( options, CFSTR( "," ) );
                    }

                    if ( CFStringGetLength( options ) )
                    {
                        CFStringTrim( options, CFSTR( "," ) );

                        CFDictionarySetValue( map, kDAMountMapMountOptionsKey, options );
                    }

                    CFRelease( options );
                }

                CFDictionarySetValue( map, kDAMountMapProbeIDKey, id );
            }

            CFRelease( id );
        }

        CFRelease( idAsString );
    }

    return map;
}

void DAMountMapListRefresh2( void )
{
    struct stat status;

    /*
     * Determine whether the mount map list is up-to-date.
     */

    if ( stat( _PATH_VSDB, &status ) )
    {
        __gDAMountMapListTime2.tv_sec  = 0;
        __gDAMountMapListTime2.tv_nsec = 0;
    }

    if ( __gDAMountMapListTime2.tv_sec  != status.st_mtimespec.tv_sec  ||
         __gDAMountMapListTime2.tv_nsec != status.st_mtimespec.tv_nsec )
    {
        __gDAMountMapListTime2.tv_sec  = status.st_mtimespec.tv_sec;
        __gDAMountMapListTime2.tv_nsec = status.st_mtimespec.tv_nsec;

        /*
         * Clear the mount map list.
         */

        CFArrayRemoveAllValues( gDAMountMapList2 );

        /*
         * Build the mount map list.
         */

        if ( setvsent( ) )
        {
            struct vsdb * item;

            while ( ( item = getvsent( ) ) )
            {
                CFDictionaryRef map;

                map = __DAMountMapCreate2( kCFAllocatorDefault, item );

                if ( map )
                {
                    CFArrayAppendValue( gDAMountMapList2, map );

                    CFRelease( map );
                }
            }

            endvsent( );
        }
    }
}

static struct timespec __gDAPreferenceListTime1 = { 0, 0 };
static struct timespec __gDAPreferenceListTime2 = { 0, 0 };

const CFStringRef kDAPreferenceMountDeferExternalKey              = CFSTR( "DAMountDeferExternal"  );
const CFStringRef kDAPreferenceMountDeferInternalKey              = CFSTR( "DAMountDeferInternal"  );
const CFStringRef kDAPreferenceMountDeferRemovableKey             = CFSTR( "DAMountDeferRemovable" );
const CFStringRef kDAPreferenceMountTrustExternalKey              = CFSTR( "DAMountTrustExternal"  );
const CFStringRef kDAPreferenceMountTrustInternalKey              = CFSTR( "DAMountTrustInternal"  );
const CFStringRef kDAPreferenceMountTrustRemovableKey             = CFSTR( "DAMountTrustRemovable" );
const CFStringRef kDAPreferenceAutoMountDisableKey                = CFSTR( "DAAutoMountDisable"    );
const CFStringRef kDAPreferenceEnableUserFSMountExternalKey       = CFSTR( "DAEnableUserFSMountExternal" );
const CFStringRef kDAPreferenceEnableUserFSMountInternalKey       = CFSTR( "DAEnableUserFSMountInternal" );
const CFStringRef kDAPreferenceEnableUserFSMountRemovableKey      = CFSTR( "DAEnableUserFSMountRemovable" );
const CFStringRef kDAPreferenceMountMethodkey                     = CFSTR( "DAMountMethod" );
const CFStringRef kDAPreferenceDisableEjectNotificationKey        = CFSTR( "DADisableEjectNotification" );
const CFStringRef kDAPreferenceDisableUnreadableNotificationKey   = CFSTR( "DADisableUnreadableNotification" );
const CFStringRef kDAPreferenceDisableUnrepairableNotificationKey = CFSTR( "DADisableUnrepairableNotification" );

void DAPreferenceListRefresh( void )
{
    struct stat status1;
    struct stat status2;

    /*
     * Determine whether the preference list is up-to-date.
     */

    if ( stat( ___PREFS_DEFAULT_DIR "/" "autodiskmount.plist", &status1 ) )
    {
        __gDAPreferenceListTime1.tv_sec  = 0;
        __gDAPreferenceListTime1.tv_nsec = 0;
    }

    if ( stat( ___PREFS_DEFAULT_DIR "/" _kDADaemonName ".plist", &status2 ) )
    {
        __gDAPreferenceListTime2.tv_sec  = 0;
        __gDAPreferenceListTime2.tv_nsec = 0;
    }

    if ( __gDAPreferenceListTime1.tv_sec  != status1.st_mtimespec.tv_sec  ||
         __gDAPreferenceListTime1.tv_nsec != status1.st_mtimespec.tv_nsec ||
         __gDAPreferenceListTime2.tv_sec  != status2.st_mtimespec.tv_sec  ||
         __gDAPreferenceListTime2.tv_nsec != status2.st_mtimespec.tv_nsec )
    {
        SCPreferencesRef preferences;

        __gDAPreferenceListTime1.tv_sec  = status1.st_mtimespec.tv_sec;
        __gDAPreferenceListTime1.tv_nsec = status1.st_mtimespec.tv_nsec;
        __gDAPreferenceListTime2.tv_sec  = status2.st_mtimespec.tv_sec;
        __gDAPreferenceListTime2.tv_nsec = status2.st_mtimespec.tv_nsec;

        /*
         * Clear the preference list.
         */

        CFDictionaryRemoveAllValues( gDAPreferenceList );

        /*
         * Build the preference list.
         */

        preferences = SCPreferencesCreate( kCFAllocatorDefault, CFSTR( "autodiskmount" ), CFSTR( "autodiskmount.plist" ) );

        if ( preferences )
        {
            CFTypeRef value;

            value = SCPreferencesGetValue( preferences, CFSTR( "AutomountDisksWithoutUserLogin" ) );

            if ( value == kCFBooleanTrue )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey,  kCFBooleanFalse );
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, kCFBooleanFalse );
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey,  kCFBooleanTrue  );
            }
            else if ( value == kCFBooleanFalse )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey,  kCFBooleanFalse );
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, kCFBooleanTrue  );
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey,  kCFBooleanTrue  );
            }

            CFRelease( preferences );
        }

        preferences = SCPreferencesCreate( kCFAllocatorDefault, CFSTR( _kDADaemonName ), CFSTR( _kDADaemonName ".plist" ) );

        if ( preferences )
        {
            CFTypeRef value;

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferExternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferInternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferRemovableKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustExternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustInternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustRemovableKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey, value );
                }
            }

            value = SCPreferencesGetValue( preferences, kDAPreferenceAutoMountDisableKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceAutoMountDisableKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceEnableUserFSMountExternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountExternalKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceEnableUserFSMountInternalKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountInternalKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceEnableUserFSMountRemovableKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceEnableUserFSMountRemovableKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceMountMethodkey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFStringGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountMethodkey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceDisableEjectNotificationKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceDisableEjectNotificationKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceDisableUnreadableNotificationKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceDisableUnreadableNotificationKey, value );
                }
            }
            
            value = SCPreferencesGetValue( preferences, kDAPreferenceDisableUnrepairableNotificationKey );

            if ( value )
            {
                if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
                {
                    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceDisableUnrepairableNotificationKey, value );
                }
            }
            
            CFRelease( preferences );
        }
    }
}

struct __DAUnit
{
    DAUnitState state;
};

typedef struct __DAUnit __DAUnit;

Boolean DAUnitGetState( DADiskRef disk, DAUnitState state )
{
    CFNumberRef key;

    key = DADiskGetDescription( disk, kDADiskDescriptionMediaBSDUnitKey );

    if ( key )
    {
        CFMutableDataRef data;

        data = ( CFMutableDataRef ) CFDictionaryGetValue( gDAUnitList, key );

        if ( data )
        {
            __DAUnit * unit;

            unit = ( void * ) CFDataGetMutableBytePtr( data );

            return ( unit->state & state ) ? TRUE : FALSE;
        }
    }

    return FALSE;
}

Boolean DAUnitGetStateRecursively( DADiskRef disk, DAUnitState state )
{
    io_service_t media;

    if ( DAUnitGetState( disk, state ) )
    {
        return TRUE;
    }

    media = DADiskGetIOMedia( disk );

    if ( media )
    {
        IOOptionBits options = kIORegistryIterateParents | kIORegistryIterateRecursively;

        while ( options )
        {
            Boolean valid = FALSE;

            while ( valid == FALSE )
            {
                io_iterator_t services = IO_OBJECT_NULL;

                IORegistryEntryCreateIterator( media, kIOServicePlane, options, &services );

                if ( services )
                {
                    io_service_t service;

                    service = IOIteratorNext( services );

                    if ( service )
                    {
                        IOObjectRelease( service );
                    }

                    while ( ( service = IOIteratorNext( services ) ) )
                    {
                        if ( IOObjectConformsTo( service, kIOMediaClass ) )
                        {
                            CFNumberRef key;

                            key = IORegistryEntryCreateCFProperty( service, CFSTR( kIOBSDUnitKey ), kCFAllocatorDefault, 0 );

                            if ( key )
                            {
                                CFMutableDataRef data;

                                data = ( CFMutableDataRef ) CFDictionaryGetValue( gDAUnitList, key );

                                if ( data )
                                {
                                    __DAUnit * unit;

                                    unit = ( void * ) CFDataGetMutableBytePtr( data );

                                    if ( ( unit->state & state ) )
                                    {
                                        CFRelease( key );
                                        IOObjectRelease( service );
                                        IOObjectRelease( services );

                                        return TRUE;
                                    }
                                }

                                CFRelease( key );
                            }
                        }
                        else
                        {
                            if ( ( options & kIORegistryIterateParents ) )
                            {
                                if ( IOObjectConformsTo( service, kIOBlockStorageDeviceClass ) )
                                {
                                    IORegistryIteratorExitEntry( services );
                                }
                            }
                        }

                        IOObjectRelease( service );
                    }

                    valid = IOIteratorIsValid( services );

                    IOObjectRelease( services );
                }
                else
                {
                    break;
                }
            }

            if ( ( options & kIORegistryIterateParents ) )
            {
                options = kIORegistryIterateRecursively;
            }
            else
            {
                options = 0;
            }
        }
    }

    return FALSE;
}

void DAUnitSetState( DADiskRef disk, DAUnitState state, Boolean value )
{
    CFNumberRef key;

    key = DADiskGetDescription( disk, kDADiskDescriptionMediaBSDUnitKey );

    if ( key )
    {
        CFMutableDataRef data;

        data = ( CFMutableDataRef ) CFDictionaryGetValue( gDAUnitList, key );

        if ( data )
        {
            __DAUnit * unit;

            unit = ( void * ) CFDataGetMutableBytePtr( data );

            unit->state &= ~state;
            unit->state |= value ? state : 0;
        }
        else
        {
            data = CFDataCreateMutable( kCFAllocatorDefault, sizeof( __DAUnit ) );

            if ( data )
            {
                __DAUnit * unit;

                unit = ( void * ) CFDataGetMutableBytePtr( data );

                unit->state = value ? state : 0;

                CFDictionarySetValue( gDAUnitList, key, data );

                CFRelease( data );
            }
        }
    }
}
