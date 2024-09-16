/*
 * Copyright (c) 1998-2018 Apple Inc. All rights reserved.
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

#include "DAMain.h"

#include "DABase.h"
#include "DADialog.h"
#include "DADisk.h"
#include "DAFileSystem.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAServer.h"
#include "DASession.h"
#include "DAStage.h"
#include "DASupport.h"
#include "DAThread.h"

#include <assert.h>
#include <dirent.h>
#include <libgen.h>
#include <notify.h>
#include <notify_keys.h>
#include <signal.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <os/variant_private.h>

static const CFStringRef __kDABundlePath = CFSTR( "/System/Library/Frameworks/DiskArbitration.framework" );

static SCDynamicStoreRef     __gDAConfigurationPort   = NULL;
static Boolean               __gDAOptionDebug         = FALSE;
static dispatch_mach_t       __gDAVolumeMountedChannel   = NULL;
static dispatch_mach_t       __gDAVolumeUnmountedChannel = NULL;
static dispatch_mach_t       __gDAVolumeUpdatedChannel   = NULL;

#if TARGET_OS_OSX
const char * kDAMainMountPointFolder           = "/Volumes";
#endif


const char * kDAMainMountPointFolderCookieFile = ".autodiskmounted";
const char * kDAMainDataVolumeMountPointFolder = "/System/Volumes/Data";

CFURLRef               gDABundlePath                   = NULL;
CFStringRef            gDAConsoleUser                  = NULL;
gid_t                  gDAConsoleUserGID               = 0;
uid_t                  gDAConsoleUserUID               = 0;
CFArrayRef             gDAConsoleUserList              = NULL;
CFMutableArrayRef      gDADiskList                     = NULL;
Boolean                gDAExit                         = FALSE;
CFMutableArrayRef      gDAFileSystemList               = NULL;
CFMutableArrayRef      gDAFileSystemProbeList          = NULL;
Boolean                gDAIdle                         = TRUE;
Boolean                gDAIdleTimerRunning             = FALSE;
CFAbsoluteTime         gDAIdleStartTime;
io_iterator_t          gDAMediaAppearedNotification    = IO_OBJECT_NULL;
io_iterator_t          gDAMediaDisappearedNotification = IO_OBJECT_NULL;
IONotificationPortRef  gDAMediaPort                    = NULL;
CFMutableArrayRef      gDAMountMapList1                = NULL;
CFMutableArrayRef      gDAMountMapList2                = NULL;
CFMutableDictionaryRef gDAPreferenceList               = NULL;
CFMutableArrayRef      gDAMountPointList               = NULL;
pid_t                  gDAProcessID                    = 0;
char *                 gDAProcessName                  = NULL;
char *                 gDAProcessNameID                = NULL;
CFMutableArrayRef      gDARequestList                  = NULL;
CFMutableArrayRef      gDAResponseList                 = NULL;
CFMutableArrayRef      gDASessionList                  = NULL;
CFMutableDictionaryRef gDAUnitList                     = NULL;
Boolean                gDAUnlockedState                = FALSE;

Boolean                gFSKitMissing                   = TRUE; // Cleared when we know FSKit is around

#if __CODECOVERAGE__
#define __segment_start_sym(_sym, _seg) extern void *_sym __asm("segment$start$" #_seg)
#define __segment_end_sym(_sym, _seg) extern void *_sym __asm("segment$end$" #_seg)
#define __section_start_sym(_sym, _seg, _sec) extern void *_sym __asm("section$start$" #_seg "$" #_sec)
#define __section_end_sym(_sym, _seg, _sec) extern void *_sym __asm("section$end$" #_seg "$" #_sec)

uint64_t __llvm_profile_get_size_for_buffer_internal(const char *DataBegin,
                                                     const char *DataEnd,
                                                     const char *CountersBegin,
                                                     const char *CountersEnd ,
                                                     const char *NamesBegin,
                                                     const char *NamesEnd);
int __llvm_profile_write_buffer_internal(char *Buffer,
                                         const char *DataBegin,
                                         const char *DataEnd,
                                         const char *CountersBegin,
                                         const char *CountersEnd ,
                                         const char *NamesBegin,
                                         const char *NamesEnd);

#define DUMP_PROFILE_DATA_SIGNAL    SIGUSR1
#define CLEAR_PROFILE_DATA_SIGNAL   SIGUSR2

#endif

static void __usage( void )
{
    /*
     * Print usage.
     */

    fprintf( stderr, "%s: [-d]\n", gDAProcessName );
    fprintf( stderr, "options:\n" );
    fprintf( stderr, "\t-d\tenable debugging\n" );

    exit( EX_USAGE );
}

static void __DACreateRootDADisk( void )
{
    struct statfs fs;
    int           status;

    status = ___statfs( "/", &fs, MNT_NOWAIT );

    if ( status == 0 )
    {
        _DADiskCreateFromFSStat( &fs );
    }
}

#if TARGET_OS_OSX
static Boolean __DAMainCreateMountPointFolder( void )
{
    /*
     * Create the mount point folder in which our mounts will be made.
     */

    int status;

    /*
     * Determine whether the mount point folder exists already.
     */

    status = access( kDAMainMountPointFolder, F_OK );

    if ( status )
    {
        /*
         * Create the mount point folder.
         */

        status = ___mkdir( kDAMainMountPointFolder, 0755 );
    }
    else
    {
        DIR * folder;

        /*
         * Correct the mount point folder's mode.
         */

        chmod( kDAMainMountPointFolder, 0755 );

        /*
         * Correct the mount point folder's ownership.
         */

        chown( kDAMainMountPointFolder, -1, ___GID_WHEEL );

        /*
         * Correct the mount point folder's contents.
         */

        folder = opendir( kDAMainMountPointFolder );

        if ( folder )
        {
            struct dirent * item;

            while ( ( item = readdir( folder ) ) )
            {
                char path[MAXPATHLEN];

                strlcpy( path, kDAMainMountPointFolder, sizeof( path ) );
                strlcat( path, "/",                     sizeof( path ) );
                strlcat( path, item->d_name,            sizeof( path ) );

                if ( item->d_type == DT_DIR )
                {
///w:start
                    char file[MAXPATHLEN];

                    strlcpy( file, path,                              sizeof( file ) );
                    strlcat( file, "/",                               sizeof( file ) );
                    strlcat( file, kDAMainMountPointFolderCookieFile, sizeof( file ) );

                    /*
                     * Remove the mount point cookie file.
                     */

                    unlink( file );
///w:stop
                    /*
                     * Remove the mount point.
                     */

                    rmdir( path );
                }
                else if ( item->d_type == DT_LNK )
                {
                    /*
                     * Remove the link.
                     */

                    unlink( path );
                }
            }

            closedir( folder );
        }
        else
        {
            status = ENOTDIR;
        }
    }

    return status ? FALSE : TRUE;
}
#endif

#if __CODECOVERAGE__
static void clear_profile_data(void)
{
    __llvm_profile_reset_counters();
}

static void dump_profile_data(void)
{
    size_t size = 0;

    __section_start_sym(sect_prf_data_start, __DATA, __llvm_prf_data);
    __section_end_sym(sect_prf_data_end, __DATA, __llvm_prf_data);
    __section_start_sym(sect_prf_cnts_start, __DATA, __llvm_prf_cnts);
    __section_end_sym(sect_prf_cnts_end, __DATA, __llvm_prf_cnts);
    __section_start_sym(sect_prf_name_start, __DATA, __llvm_prf_names);
    __section_end_sym(sect_prf_name_end, __DATA, __llvm_prf_names);

    size = __llvm_profile_get_size_for_buffer_internal((const char *) &sect_prf_data_start, (const char *) &sect_prf_data_end,
                                                       (const uint64_t *) &sect_prf_cnts_start,            (const uint64_t *) &sect_prf_cnts_end,
                                                       (const char *) &sect_prf_name_start,                (const char *)&sect_prf_name_end);
    void *outputBuffer = malloc(size);
    if (outputBuffer == NULL) goto exit;

    int err = 0;
    memset(outputBuffer, 0x00, size);

    err = __llvm_profile_write_buffer_internal((char *)outputBuffer,
                                               (const char *) &sect_prf_data_start, (const char *) &sect_prf_data_end,
                                               (const uint64_t *) &sect_prf_cnts_start,            (const uint64_t *) &sect_prf_cnts_end,
                                               (const char *) &sect_prf_name_start,                (const char *)&sect_prf_name_end);

    if (err) goto exit;

    int f = open ( "/tmp/diskarb.profraw", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if (f<0) goto exit;

    write(f, outputBuffer, size);
exit:
    if (f>=0) close (f);
    if (outputBuffer) free(outputBuffer);
}
#endif

static void __DAMainSignal( int sig )
{
    /*
     * Process SIGTERM/SIGUSR1/SIGUSR2 signals.
     * DUMP_PROFILE_DATA_SIGNAL(SIGUSR1) is used to dump the llvm code coverage data into /tmp/diskarb.profraw
     * CLEAR_PROFILE_DATA_SIGNAL(SIGUSR2) is used to clear the llvm code coverage data
     */

    if (sig == SIGTERM)
    {
        gDAExit = TRUE;
    }
#if __CODECOVERAGE__
    else if (sig == DUMP_PROFILE_DATA_SIGNAL)
    {
        dump_profile_data();
    }
    else
    {
        clear_profile_data();
    }
#endif
}

static void __DAMain( void *__unused context )
{
    FILE *             file;
    CFStringRef        key;
    CFMutableArrayRef  keys;
    char               path[MAXPATHLEN];
    mach_port_t        port;
    dispatch_mach_t    machChannel;
    int                token;

    /*
     * Initialize classes.
     */

    DADiskInitialize( );

    DAFileSystemInitialize( );

    DASessionInitialize( );

    /*
     * Initialize bundle path.
     */

    gDABundlePath = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, __kDABundlePath, kCFURLPOSIXPathStyle, TRUE );

    assert( gDABundlePath );

    /*
     * Initialize console user.
     */
#if TARGET_OS_OSX
    gDAConsoleUser     = ___SCDynamicStoreCopyConsoleUser( NULL, &gDAConsoleUserUID, &gDAConsoleUserGID );
    gDAConsoleUserList = ___SCDynamicStoreCopyConsoleInformation( NULL );
#endif
    /*
     * Initialize log.
     */

    DALogOpen( gDAProcessName, __gDAOptionDebug, TRUE );

    /*
     * Initialize process ID.
     */

    gDAProcessID = getpid( );

    /*
     * Initialize process ID tag.
     */

    asprintf( &gDAProcessNameID, "%s [%d]", gDAProcessName, gDAProcessID );

    assert( gDAProcessNameID );

    /*
     * Create the disk list.
     */

    gDADiskList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDADiskList );

    /*
     * Create the file system list.
     */

    gDAFileSystemList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAFileSystemList );

    /*
     * Create the file system probe list.
     */

    gDAFileSystemProbeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAFileSystemProbeList );

    /*
     * Create the mount map list.
     */

    gDAMountMapList1 = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAMountMapList1 );

    /*
     * Create the mount map list.
     */

    gDAMountMapList2 = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAMountMapList2 );

    /*
     * Create the preference list.
     */

    gDAPreferenceList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( gDAPreferenceList );
    
    /*
     * Create the mount  list.
     */

    gDAMountPointList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAMountPointList );

    /*
     * Create the request list.
     */

    gDARequestList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDARequestList );

    /*
     * Create the response list.
     */

    gDAResponseList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAResponseList );

    /*
     * Create the session list.
     */

    gDASessionList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDASessionList );

    /*
     * Create the unit list.
     */

    gDAUnitList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( gDAUnitList );

    /*
     * Create the Disk Arbitration master run loop source.
     */

    DAServerInit();

    /*
     * Create the I/O Kit notification run loop source.
     */

    gDAMediaPort = IONotificationPortCreate( kIOMainPortDefault );

    if ( gDAMediaPort == NULL )
    {
        DALogError( "could not create I/O Kit notification port." );
        exit( EX_SOFTWARE );
    }
    
    IONotificationPortSetDispatchQueue( gDAMediaPort, DAServerWorkLoop() );

#if TARGET_OS_OSX

    /*
     * Create the System Configuration notification run loop source.
     */

    __gDAConfigurationPort = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR( _kDADaemonName ), _DAConfigurationCallback, NULL );

    if ( __gDAConfigurationPort == NULL )
    {
        DALogError( "could not create System Configuration notification port." );
        exit( EX_SOFTWARE );
    }

    SCDynamicStoreSetDispatchQueue(__gDAConfigurationPort, DAServerWorkLoop() );

#endif
    /*
     * Create the file system run loop source.
     */

    machChannel = DAFileSystemCreateMachChannel();

    if ( machChannel == NULL )
    {
        DALogError( "could not create file system run loop channel." );
        exit( EX_SOFTWARE );
    }

    
    /*
     * Create the thread run loop source.
     */

    machChannel = DAThreadCreateMachChannel( );

    if ( machChannel == NULL )
    {
        DALogError( "could not create thread run loop channel." );
        exit( EX_SOFTWARE );
    }


    /*
     * Create the "media disappeared" notification.
     */

    IOServiceAddMatchingNotification( gDAMediaPort,
                                      kIOTerminatedNotification,
                                      IOServiceMatching( kIOMediaClass ),
                                      _DAMediaDisappearedCallback,
                                      NULL,
                                      &gDAMediaDisappearedNotification );

    if ( gDAMediaDisappearedNotification == IO_OBJECT_NULL )
    {
        DALogError( "could not create \"media disappeared\" notification." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the "media appeared" notification.
     */

    IOServiceAddMatchingNotification( gDAMediaPort,
                                      kIOMatchedNotification,
                                      IOServiceMatching( kIOMediaClass ),
                                      _DAMediaAppearedCallback,
                                      NULL,
                                      &gDAMediaAppearedNotification );

    if ( gDAMediaAppearedNotification == IO_OBJECT_NULL )
    {
        DALogError( "could not create \"media appeared\" notification." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the "configuration changed" notification.
     */
#if TARGET_OS_OSX
   key  = SCDynamicStoreKeyCreateConsoleUser( kCFAllocatorDefault );
 keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( key  );
    assert( keys );

    CFArrayAppendValue( keys, key );

    if ( SCDynamicStoreSetNotificationKeys( __gDAConfigurationPort, keys, NULL ) == FALSE )
    {
        DALogError( "could not create \"configuration changed\" notification." );
        exit( EX_SOFTWARE );
    }

    CFRelease( key  );
    CFRelease( keys );
#endif
    
#if TARGET_OS_IOS
    if ( os_variant_has_factory_content( "com.apple.diskarbitrationd" ) == false )
    {
        DARegisterForFirstUnlockNotification();
    }
#endif
    
#if TARGET_OS_OSX
    /*
     * Create the "file system mounted" notification.
     */

    if ( notify_register_mach_port( kNotifyVFSMount, &port, 0, &token ) )
    {
        DALogError( "could not create \"file system mounted\" notification." );
        exit( EX_SOFTWARE );
    }

    __gDAVolumeMountedChannel = dispatch_mach_create_f("diskarbitrationd/volumemount", DAServerWorkLoop(), NULL, _DAVolumeMountedMachHandler );
    
    dispatch_mach_connect(__gDAVolumeMountedChannel, port, MACH_PORT_NULL, NULL);
    /*
     * Create the "file system unmounted" notification.
     */

    if ( notify_register_mach_port( kNotifyVFSUnmount, &port, 0, &token ) )
    {
        DALogError( "could not create \"file system unmounted\" notification." );
        exit( EX_SOFTWARE );
    }

    __gDAVolumeUnmountedChannel = dispatch_mach_create_f("diskarbitrationd/volumeunmount", DAServerWorkLoop(), NULL, _DAVolumeUnmountedMachHandler );
    
    dispatch_mach_connect(__gDAVolumeUnmountedChannel, port, MACH_PORT_NULL, NULL);
    
    /*
     * Create the "file system updated" notification.
     */

    if ( notify_register_mach_port( kNotifyVFSUpdate, &port, 0, &token ) )
    {
        DALogError( "could not create \"file system updated\" notification." );
        exit( EX_SOFTWARE );
    }

    __gDAVolumeUpdatedChannel =  dispatch_mach_create_f("diskarbitrationd/volumeupdate", DAServerWorkLoop(), NULL, _DAVolumeUpdatedMachHandler );
    
    dispatch_mach_connect(__gDAVolumeUpdatedChannel, port, MACH_PORT_NULL, NULL);
#else
    /*
     * Use VFS notifications since KernelEventAgent does not exist in embedded platforms.
     */
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VFS,
                                                      0,
                                                      DISPATCH_VFS_MOUNT | DISPATCH_VFS_UNMOUNT | DISPATCH_VFS_UPDATE,
                                                      DAServerWorkLoop());
    dispatch_source_set_event_handler(source, ^{
        int64_t event_flags = dispatch_source_get_data(source);
        if (event_flags & VQ_MOUNT)
        {
            _DAVolumeMountedCallback();
        }
        if (event_flags & VQ_UNMOUNT)
        {
            _DAVolumeUnmountedCallback();
        }
        if (event_flags & VQ_UPDATE)
        {
            _DAVolumeUpdatedCallback();
        }
    });
    dispatch_resume( source );
    
#endif
    /*
     * Create the mount point folder.
     */
#if TARGET_OS_OSX
    if ( __DAMainCreateMountPointFolder( ) == FALSE )
    {
        DALogError( "could not create mount point folder." );
        exit( EX_SOFTWARE );
    }
#endif
    /*
     * Create the process ID file.
     */

    snprintf( path, sizeof( path ), "/var/run/%s.pid", gDAProcessName );

    file = fopen( path, "w" );

    if ( file )
    {
        fprintf( file, "%d\n", gDAProcessID );
        fclose( file );
    }

    /*
     * Announce our arrival in the debug log.
     */

    DALogInfo( "" );
    DALogInfo( "server has been started." );

    if ( gDAConsoleUser )
    {
        DALogInfo( "  console user = %@ [%d].", gDAConsoleUser, gDAConsoleUserUID );
    }
    else
    {
           DALogInfo( "  console user = none." );
    }

    /*
     * Freshen the preference list.
     */

    DAPreferenceListRefresh( );

    /*
     * If FSKit is defined, check to see if it's here
     */
#ifdef DA_FSKIT
    DACheckForFSKit();
#endif

    /*
     * Freshen the file system list.
     */

    DAFileSystemListRefresh( );

    /*
     * Freshen the mount map list.
     */

    DAMountMapListRefresh1( );

    /*
     * Freshen the mount map list.
     */

    DAMountMapListRefresh2( );

    /*
     * Process the initial set of media objects in I/O Kit.
     */

    _DAMediaDisappearedCallback( NULL, gDAMediaDisappearedNotification );

    _DAMediaAppearedCallback( NULL, gDAMediaAppearedNotification );
    

#if !TARGET_OS_OSX
    
    __DACreateRootDADisk( );
	xpc_set_event_stream_handler("com.apple.iokit.matching",
                                 DAServerWorkLoop(),
                                 ^(xpc_object_t event)
    {
        DALogDebug( "iokit.matching: got event object to process:");
    });
#endif
    notify_post("com.apple.diskarbitrationd.launched");
}

int main( int argc, char * argv[], char * envp[] )
{
    /*
     * Start.
     */

    char option;

    /*
     * Initialize.
     */

    gDAProcessName = basename( argv[0] );

    /*
     * Check credentials.
     */

    if ( geteuid( ) )
    {
        fprintf( stderr, "%s: permission denied.\n", gDAProcessName );

        exit( EX_NOPERM );
    }

    /*
     * Process arguments.
     */

    while ( ( option = getopt( argc, argv, "d" ) ) != -1 )
    {
        switch ( option )
        {
            case 'd':
            {
                __gDAOptionDebug = TRUE;

                break;
            }
            default:
            {
                __usage( );

                break;
            }
        }
    }

    /*
     * Continue to start up.
     */

    signal( SIGTERM, __DAMainSignal );
#if __CODECOVERAGE__
    signal( DUMP_PROFILE_DATA_SIGNAL, __DAMainSignal );
    signal( CLEAR_PROFILE_DATA_SIGNAL, __DAMainSignal );
#endif

    
    dispatch_async_and_wait_f(DAServerWorkLoop(), NULL, __DAMain);
    dispatch_main();

    exit( EX_OK );
}
