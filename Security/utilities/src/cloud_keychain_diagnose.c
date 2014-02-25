/*
 * clang cloud_keychain_diagnose.c -laks -framework CoreFoundation -framework IOKit -framework Security -o /tmp/cloud_keychain_diagnose
 */


#if !TARGET_OS_EMBEDDED
#include "sec/Security/SecBase.h"
#include "sec/Security/SecKey.h"
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

#if !TARGET_IPHONE_SIMULATOR

/* Header Declarations */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <asl.h>
#include <asl_msg.h>

#if TARGET_OS_EMBEDDED
#include <asl_core.h>
#endif

#include <string.h>
#include <errno.h>
#include <libaks.h>

#include "SOSCloudCircle.h"
#include "SOSPeerInfo.h"


/* Constant Declarations */
#define SUCCESS 0
#define FAILURE -1

#define MAX_PATH_LEN                1024
#define SUFFIX_LENGTH               4
#define BUFFER_SIZE                 1024
#define MAX_DATA_RATE               32

/* External CloudKeychain Bridge Types */
typedef void (^CloudKeychainReplyBlock)(CFDictionaryRef returnedValues, CFErrorRef error);
extern void SOSCloudKeychainGetAllObjectsFromCloud(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

/* External AppleKeyStore Types */
enum {
 my_keybag_state_bio_unlock = 1 << 3
};


/* Dictionary Dump State */
struct dict_dump_state
{
    FILE                *log_file;
    CFDictionaryRef     dict;
    unsigned int        indent_level;
};

/* Static Function Declarations */
static
void
usage();

static
int
gather_diagnostics();

static
int
enable_cloud_keychain_diagnostics(
    const unsigned int enable_flag);

static
int
build_log_path(
    char *log_path);

static
int
dump_system_information(
    FILE *log_file);

static
int
dump_circle_state(
    FILE *log_file);

static
int
dump_keychain_sync_kvs(
    FILE *log_file);

static
void
dump_dict(
    FILE *log_file,
    CFDictionaryRef dict,
    const unsigned int indent_level);

static
void
dump_dict_applier(
    const void *key,
    const void *value,
    void *context);

static
int
dump_asl_sender(
    FILE *log_file,
    const char *asl_sender);

static
void
dump_cferror(
    FILE *log_file,
    const char *description,
    CFErrorRef error);

/* Function Definitions */
int
main(
    int argc,
    char **argv)
{
    int result = EXIT_FAILURE;
    
    /* Parse the arguments. */
    if (argc > 2) {
    
        usage();
    }

    /* Should we just gather logs and status? */
    if (argc == 1) {
    
        if (gather_diagnostics()) {
            
            fprintf(stderr, "Could not gather diagnostics\n");
            goto BAIL;
        }        
    } else {
    
        /* Should we enable or disable logging? */
        if (strncmp(argv[1], "enable", 6) == 0) {
        
            /* Enable. */
            if (enable_cloud_keychain_diagnostics(1)) {
            
                fprintf(stderr, "Could not enable additional cloud keychain diagnostics\n");
                goto BAIL;
            }            
        } else if (strncmp(argv[1], "disable", 7) == 0) {

            /* Enable. */
            if (enable_cloud_keychain_diagnostics(1)) {
            
                fprintf(stderr, "Could not disable additional cloud keychain diagnostics\n");
                goto BAIL;
            } 
        } else {
        
            /* Get a job, hippy. */
            usage();
        }
    }
    
    /* Set the exit status to success. */
    result = EXIT_FAILURE;
    
BAIL:

    return result;
}

/* Static Function Definitions */
static
void
usage()
{
    fprintf(stderr, "usage: cloud_keychain_diagnose [enable|disable]\n");
    exit(EXIT_FAILURE);
}

static
int
gather_diagnostics()
{
    int     result = FAILURE;
    char    log_path[MAX_PATH_LEN] = "";
    int     log_fd = -1;
    FILE    *log_file = NULL;
    
    /*
     * Create the diagnostics file.
     *
     * Dump the system information.
     *     on OS X, defaults read if the shim is active
     * Dump the circle state.
     * Dump the raw KVS data.
     * Dump known ASL logs
     *
     * Remaining work to do from rdar://12479351
     * grab the syslog
     * query for all items with sync=1
     * enable KVS logging
     * enable push notification logging
     */
    
    /* Build the log path. */
    if (build_log_path(log_path)) {
    
        fprintf(stderr, "Could not build the log path\n");
        goto BAIL;
    }
    
    /* Create it with a randomized suffix. */
    log_fd = mkstemps(log_path, SUFFIX_LENGTH);
    if (log_fd == -1) {
    
        fprintf(stderr, "Could not create the log file: %s\n", strerror(errno));
        goto BAIL;
    }
    
    /* Create a file object from the descriptor. */
    log_file = fdopen(log_fd, "w");
    if (log_file == NULL) {
    
        fprintf(stderr, "Could not recreate the log file: %s\n", strerror(errno));
        goto BAIL;
    }
    
    log_fd = -1;
    
    printf("Writing cloud keychain diagnostics to %s\n", log_path);
    
    /* Dump the system information. */
    if (dump_system_information(log_file)) {
    
        fprintf(stderr, "Could not dump the system information\n");
        goto BAIL;
    }
    
    /* Dump the SOS circle state. */
    if (dump_circle_state(log_file)) {
    
        fprintf(stderr, "Could not dump the SOS circle state\n");
        goto BAIL;
    }
    
    /* Dump the raw keychain syncing KVS. */
    if (dump_keychain_sync_kvs(log_file)) {
    
        fprintf(stderr, "Could not the raw keychain syncing KVS\n");
        goto BAIL;
    }
    
    /* 
     * Dump the various and sundry ASL logs.
     */
    
    if (dump_asl_sender(log_file, "com.apple.kb-service")) {
    
        fprintf(stderr, "Could not dump the ASL log for com.apple.kb-service\n");
        goto BAIL;
    }
    
    if (dump_asl_sender(log_file, "com.apple.securityd")) {
    
        fprintf(stderr, "Could not dump the ASL log for com.apple.securityd\n");
        goto BAIL;
    }
    
    if (dump_asl_sender(log_file, "com.apple.secd")) {
    
        fprintf(stderr, "Could not dump the ASL log for com.apple.secd\n");
        goto BAIL;
    }
    
    if (dump_asl_sender(log_file, "CloudKeychainProxy")) {
    
        fprintf(stderr, "Could not dump the ASL log for CloudKeychainProxy\n");
        goto BAIL;
    }
    
    if (dump_asl_sender(log_file, "securityd")) {
    
        fprintf(stderr, "Could not dump the ASL log for securityd\n");
        goto BAIL;
    }
    
    if (dump_asl_sender(log_file, "secd")) {
    
        fprintf(stderr, "Could not dump the ASL log for secd\n");
        goto BAIL;
    }

    /* Set the result to success. */
    result = SUCCESS;

BAIL:

    /* Close the diagnostics file? */
    if (log_file != NULL) {
    
        fclose(log_file);
        log_file = NULL;
    }
    
    /* Close the diagnostics file descriptor? */
    if (log_fd != -1) {
    
        close(log_fd);
        log_fd = -1;
    }
    
    return result;
}

static
int
enable_cloud_keychain_diagnostics(
    const unsigned int enable_flag)
{
    int result = FAILURE;
    
    /* Set the result to success. */
    result = SUCCESS;
    
    return result;
}

static
int
build_log_path(
    char *log_path)
{
    int             result = FAILURE;
    time_t          now;
    struct tm       *time_cube;
    CFDictionaryRef system_version_dict = NULL;
    CFStringRef     product_name = NULL;
    
    /* Get the current time. */
    now = time(NULL);
    
    /* Convert the time into something usable. */
    time_cube = localtime(&now);
    if (time_cube == NULL) {
    
        fprintf(stderr, "I don't know what time it is.\n");
        goto BAIL;
    }
    
    /* Copy the system version dictionary. */
    system_version_dict = _CFCopySystemVersionDictionary();
    if (system_version_dict == NULL) {
    
        fprintf(stderr, "Could not copy the system version dictionary\n");
        goto BAIL;
    }
    
    /* Extract the product name. */
    product_name = CFDictionaryGetValue(system_version_dict, _kCFSystemVersionProductNameKey);
    if (product_name == NULL) {
    
        fprintf(stderr, "Could not extract the product name from the system version dictionary\n");
        goto BAIL;
    }

    /* Is this a Mac? */
    if (CFEqual(product_name, CFSTR("Mac OS X"))) {
    
        /* Prepare the file template to go into /tmp. */
        snprintf(
            log_path,
            MAX_PATH_LEN,
            "/tmp/cloud_keychain_diagnostics.%d_%d_%d.%d%d%d.XXXX.txt",
            1900 + time_cube->tm_year,
            time_cube->tm_mon,
            time_cube->tm_mday,
            time_cube->tm_hour,
            time_cube->tm_min,
            time_cube->tm_sec);
    } else {
    
        /* Prepare the file template to go into CrashReporter. */
        snprintf(
            log_path,
            MAX_PATH_LEN,
            "/Library/Logs/CrashReporter/cloud_keychain_diagnostics.%d_%d_%d.%d%d%d.XXXX.txt",
            1900 + time_cube->tm_year,
            time_cube->tm_mon,
            time_cube->tm_mday,
            time_cube->tm_hour,
            time_cube->tm_min,
            time_cube->tm_sec);
    }
    
    /* Set the result to success. */
    result = SUCCESS;
    
BAIL:

    /* Release the system version dictionary? */
    if (system_version_dict != NULL) {
    
        CFRelease(system_version_dict);
        system_version_dict = NULL;
    }
    
    return result;
}

static
int
dump_system_information(
    FILE *log_file)
{
    int             result = FAILURE;
    CFDictionaryRef dict = NULL;
    char            buffer[BUFFER_SIZE];
    CFStringRef     product_name;
    CFStringRef     product_version;
    CFStringRef     product_build_version;
    time_t          now;
    CFTypeRef       shim_flag = NULL;
    int             keybag_handle = bad_keybag_handle;
    kern_return_t   kr = 0;
    keybag_state_t  keybag_state = 0;
    
    /*
     * Dump the system information.
     *  ProductName
     *  ProductVersion
     *  ProductBuildVersion
     *  Host name
     */
    
    /* Dump a header. */
    fprintf(log_file, "Host Information:\n");
    fprintf(log_file, "=================\n");
    
    /* Copy the system version dictionary. */
    dict = _CFCopySystemVersionDictionary();
    if (dict == NULL) {
    
        fprintf(stderr, "Could not copy the system version dictionary\n");
        goto BAIL;
    }
    
    /* Extract the product name. */
    product_name = CFDictionaryGetValue(dict, _kCFSystemVersionProductNameKey);
    if (product_name == NULL) {
    
        fprintf(stderr, "Could not extract the product name from the system version dictionary\n");
        goto BAIL;
    }
    
    /* Convert the product name to a C string. */
    if (!CFStringGetCString(product_name, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
    
        fprintf(stderr, "Could not convert the product name to a C string\n");
        goto BAIL;
    }
    
    /* Dump the product name. */
    fprintf(log_file, "Product Name: %s\n", buffer);
    
    /* Extract the product version. */
    product_version = CFDictionaryGetValue(dict, _kCFSystemVersionProductVersionKey);
    if (product_version == NULL) {
    
        fprintf(stderr, "Could not extract the product version from the system version dictionary\n");
        goto BAIL;
    }
    
    /* Convert the product version to a C string. */
    if (!CFStringGetCString(product_version, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
    
        fprintf(stderr, "Could not convert the product version to a C string\n");
        goto BAIL;
    }
    
    /* Dump the product version */
    fprintf(log_file, "Product Version: %s\n", buffer);
    
    /* Extract the product build version. */
    product_build_version = CFDictionaryGetValue(dict, _kCFSystemVersionBuildVersionKey);
    if (product_build_version == NULL) {
    
        fprintf(stderr, "Could not extract the product build version from the system version dictionary\n");
        goto BAIL;
    }
    
    /* Convert the product build version to a C string. */
    if (!CFStringGetCString(product_build_version, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
    
        fprintf(stderr, "Could not convert the product build version to a C string\n");
        goto BAIL;
    }
    
    /* Dump the product build version. */
    fprintf(log_file, "Product Build Version: %s\n", buffer);
    
    /* Lookup the host name. */
    if (gethostname(buffer, BUFFER_SIZE) == -1) {
    
        fprintf(stderr, "Could not lookup the host name\n");
        goto BAIL;
    }
    
    /* Dump the host name. */
    fprintf(log_file, "Host Name: %s\n", buffer);
    
    /* Lookup the current time. */
    if (gethostname(buffer, BUFFER_SIZE) == -1) {
    
        fprintf(stderr, "Could not lookup the host name\n");
        goto BAIL;
    }

    /* Get the current time. */
    now = time(NULL);
    
    /* Dump the current time. */
    fprintf(log_file, "Time: %s", ctime(&now));
    
    /* Is this a Mac? */
    if (CFEqual(product_name, CFSTR("Mac OS X"))) {
    
        /* Set the keybag handle. */
        keybag_handle = session_keybag_handle;
        
        /* Lookup the state of the shim. */
        shim_flag = (CFNumberRef)CFPreferencesCopyValue(CFSTR("SecItemSynchronizable"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (shim_flag && CFGetTypeID(shim_flag) == CFBooleanGetTypeID()) {
        
            /* Is the shim enabled? */
            if (CFBooleanGetValue((CFBooleanRef)shim_flag)) {
            
                fprintf(log_file, "The SecItem shim is enabled\n");
            } else {
                
                fprintf(log_file, "The SecItem shim is disabled\n");
            }
        } else {
        
            fprintf(log_file, "The SecItem shim is disabled\n");
        }
    } else {
    
        /* Set the keybag handle. */
        keybag_handle = device_keybag_handle;
    }
    
    /* Get the keybag state. */
    kr = aks_get_lock_state(keybag_handle, &keybag_state);
    if (kr) {
    
        fprintf(stderr, "Could not call aks_get_lock_state\n");
    } else {
    
        switch (keybag_state) {
        
            case keybag_state_unlocked: {
                
                fprintf(log_file, "Keybag State: Unlocked\n");
            }break;
            
            case keybag_state_locked: {
                
                fprintf(log_file, "Keybag State: Locked\n");
            }break;
            
            case keybag_state_no_pin: {
                
                fprintf(log_file, "Keybag State: No Passcode\n");
            }break;
            
            case keybag_state_been_unlocked: {
                
                fprintf(log_file, "Keybag State: Been Unlocked\n");
            }break;
            
            case my_keybag_state_bio_unlock: {
                
                fprintf(log_file, "Keybag State: Bio Unlock\n");
            }break;
            
            default: {
            
                fprintf(log_file, "Keybag State: UNKNOWN\n");
            }break;
        }
    }
    
    /* Dump a footer. */
    fprintf(log_file, "=================\n\n");

    /* Set the result to success. */
    result = SUCCESS;
    
BAIL:

    /* Release the shim flag? */
    if (shim_flag) {
    
        CFRelease(shim_flag);
        shim_flag = NULL;
    }
    
    /* Release the system version dictionary? */
    if (dict != NULL) {
    
        CFRelease(dict);
        dict = NULL;
    }
    
    return result;
}

static
int
dump_circle_state(
    FILE *log_file)
{
    int             result = FAILURE;
    CFErrorRef      error = NULL;
    SOSCCStatus     circle_status;
    char            *circle_state_string = NULL;
    CFArrayRef      peer_list = NULL;
    CFIndex         num_peers;
    CFIndex         i;
    SOSPeerInfoRef  peer_info;
    CFDictionaryRef peer_gestalt = NULL;
    CFStringRef     peer_name;
    CFStringRef     peer_device_type;
    CFStringRef     peerID;
    char            buffer[BUFFER_SIZE];

    /*
     * Dump the SOS circle state.
     */
    
    /* Dump a header. */
    fprintf(log_file, "SOS Circle State:\n");
    fprintf(log_file, "=================\n");

    /* Are we in a circle? */
    circle_status = SOSCCThisDeviceIsInCircle(&error);
    if (error != NULL) {
    
        /* Dump and consume the error. */
        dump_cferror(log_file, "Could not call SOSCCThisDeviceIsInCircle", error);
    } else {
    
        switch (circle_status) {
        
            case kSOSCCInCircle: {
                circle_state_string = "kSOSCCInCircle";
            }break;
            
            case kSOSCCNotInCircle: {
                circle_state_string = "kSOSCCNotInCircle";
            }break;
            
            case kSOSCCRequestPending: {
                circle_state_string = "kSOSCCRequestPending";
            }break;
            
            case kSOSCCCircleAbsent: {
                circle_state_string = "kSOSCCCircleAbsent";
            }break;
            
            case kSOSCCError: {
                circle_state_string = "kSOSCCError";
            }break;
            
            case kSOSCCParamErr: {
                circle_state_string = "kSOSCCParamErr";
            }break;
            
            case kSOSCCMemoryErr: {
                circle_state_string = "kSOSCCMemoryErr";
            }break;
    
            default: {
                circle_state_string = "Unknown circle status?";
            }
        }
        
        fprintf(log_file, "Circle Status: %s\n", circle_state_string);
    }
    
    /* Can we authenticate? */
    if (!SOSCCCanAuthenticate(&error)) {

        if (error) {
        
            /* Dump and consume the error. */
            dump_cferror(log_file, "Could not call SOSCCCanAuthenticate", error);
        } else {

            fprintf(log_file, "Can Authenticate: NO\n");
        }
    } else {
    
        fprintf(log_file, "Can Authenticate: YES\n");
    }
    
    /* Copy the peers. */
    peer_list = SOSCCCopyPeerPeerInfo(&error);
    if (error) {

        /* Dump the error. */
        dump_cferror(log_file, "Could not call SOSCCCopyPeerPeerInfo", error);
    } else {
    
        /* Get the number of peers. */
        num_peers = CFArrayGetCount(peer_list);
        
        fprintf(log_file, "Number of syncing peers: %ld\n", num_peers);
        
        if (num_peers > 0) {
        
            fprintf(log_file, "\n");
        }
        
        /* Enumerate the peers. */
        for (i = 0; i < num_peers; i++) {
        
            peer_info = (SOSPeerInfoRef) CFArrayGetValueAtIndex(peer_list, i);
            if (peer_info == NULL) {
            
                fprintf(stderr, "Could not extract peer %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /*
            peer_gestalt = SOSPeerInfoCopyPeerGestalt(peer_info);
            if (peer_gestalt == NULL) {

                fprintf(stderr, "Could not copy peer gestalt %ld of %ld\n", i, num_peers);
                goto BAIL;            
            }
            */
            
            /* Get the peer name. */
            peer_name = SOSPeerInfoGetPeerName(peer_info);
            if (peer_name == NULL) {
            
                fprintf(stderr, "Could not extract peer name %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Convert the peer name to a C string. */
            if (!CFStringGetCString(peer_name, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer name to a C string\n");
                goto BAIL;
            }
    
            /* Dump the peer name. */
            fprintf(log_file, " Peer Name: %s\n", buffer);
            
            /* Get the peer device type. */
            peer_device_type = SOSPeerInfoGetPeerDeviceType(peer_info);
            if (peer_device_type == NULL) {
            
                fprintf(stderr, "Could not extract peer device type %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Convert the peer device type to a C string. */
            if (!CFStringGetCString(peer_device_type, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer device type to a C string\n");
                goto BAIL;
            }
            
            /* Dump the peer name. */
            fprintf(log_file, " Peer Device Type: %s\n", buffer);
            
            /* Get the peer ID. */            
            peerID = SOSPeerInfoGetPeerID(peer_info);
            if (peerID == NULL) {
            
                fprintf(stderr, "Could not extract peer ID %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Dump the peer name. */
            fprintf(log_file, " Peer ID: %s\n", buffer);
            
            /* Convert the peer ID to a C string. */
            if (!CFStringGetCString(peerID, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer ID to a C string\n");
                goto BAIL;
            }
            
            /* Make it pretty. */
            fprintf(log_file, "\n");
        }
        
        /* Release the peer list. */
        CFRelease(peer_list);
        peer_list = NULL;
    }

    /* Copy the applicant peers. */
    peer_list = SOSCCCopyApplicantPeerInfo(&error);
    if (error) {

        /* Dump the error. */
        dump_cferror(log_file, "Could not call SOSCCCopyApplicantPeerInfo", error);
    } else {
    
        /* Get the number of peers. */
        num_peers = CFArrayGetCount(peer_list);
        
        fprintf(log_file, "Number of applicant peers: %ld\n", num_peers);
        
        if (num_peers > 0) {
        
            fprintf(log_file, "\n");
        }
        
        /* Enumerate the peers. */
        for (i = 0; i < num_peers; i++) {
        
            peer_info = (SOSPeerInfoRef) CFArrayGetValueAtIndex(peer_list, i);
            if (peer_info == NULL) {
            
                fprintf(stderr, "Could not extract peer %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /*
            peer_gestalt = SOSPeerInfoCopyPeerGestalt(peer_info);
            if (peer_gestalt == NULL) {

                fprintf(stderr, "Could not copy peer gestalt %ld of %ld\n", i, num_peers);
                goto BAIL;            
            }
            */
            
            /* Get the peer name. */
            peer_name = SOSPeerInfoGetPeerName(peer_info);
            if (peer_name == NULL) {
            
                fprintf(stderr, "Could not extract peer name %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Convert the peer name to a C string. */
            if (!CFStringGetCString(peer_name, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer name to a C string\n");
                goto BAIL;
            }
    
            /* Dump the peer name. */
            fprintf(log_file, " Applicant Name: %s\n", buffer);
            
            /* Get the peer device type. */
            peer_device_type = SOSPeerInfoGetPeerDeviceType(peer_info);
            if (peer_device_type == NULL) {
            
                fprintf(stderr, "Could not extract peer device type %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Convert the peer device type to a C string. */
            if (!CFStringGetCString(peer_device_type, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer device type to a C string\n");
                goto BAIL;
            }
            
            /* Dump the peer name. */
            fprintf(log_file, " Applicant Device Type: %s\n", buffer);
            
            /* Get the peer ID. */            
            peerID = SOSPeerInfoGetPeerID(peer_info);
            if (peerID == NULL) {
            
                fprintf(stderr, "Could not extract peer ID %ld of %ld\n", i, num_peers);
                goto BAIL;
            }
            
            /* Dump the peer name. */
            fprintf(log_file, " Applicant ID: %s\n", buffer);
            
            /* Convert the peer ID to a C string. */
            if (!CFStringGetCString(peerID, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
            
                fprintf(stderr, "Could not convert the peer ID to a C string\n");
                goto BAIL;
            }
            
            /* Make it pretty. */
            if (i < num_peers - 1) {
            
                fprintf(log_file, "\n");
            }
        }
        
        /* Release the peer list. */
        CFRelease(peer_list);
        peer_list = NULL;
    }
    
    /* Dump a footer. */
    fprintf(log_file, "=================\n\n");

    /* Set the result to success. */
    result = SUCCESS;
    
BAIL:
    
    /* Release the peer gestalt? */
    if (peer_gestalt != NULL) {
    
        CFRelease(peer_gestalt);
        peer_gestalt = NULL;
    }
    
    /* Release the peer list? */
    if (peer_list != NULL) {
    
        CFRelease(peer_list);
        peer_list = NULL;
    }
    
    /* Release the error string? */
    if (error != NULL) {
    
        CFRelease(error);
        error = NULL;
    }
    
    return result;
}

static
int
dump_keychain_sync_kvs(
    FILE *log_file)
{
    int                     result = FAILURE;
    dispatch_group_t        cloud_group;
    dispatch_queue_t        cloud_queue;
    dispatch_semaphore_t    waitSemaphore;
    dispatch_time_t         finishTime;
    __block CFDictionaryRef kvs_dict = NULL;

    /*
     * Dump the keychain syncing KVS.
     */
    
    /* Dump a header. */
    fprintf(log_file, "Keychain Syncing KVS:\n");
    fprintf(log_file, "=================\n");
    
    /* Create the serial dispatch queue to talk to CloudKeychainProxy. */
    cloud_queue = dispatch_queue_create("cloud_queue", DISPATCH_QUEUE_SERIAL);

    /* Create a semaphore. */
    waitSemaphore = dispatch_semaphore_create(0);
    
    /* Create the finish time. */
    finishTime = dispatch_time(DISPATCH_TIME_NOW, 30ull * NSEC_PER_SEC);
    
    /* Create the dispatch group. */
    cloud_group = dispatch_group_create();

    /* Enter the dispatch group. */
    dispatch_group_enter(cloud_group);

    /* Establish the CloudKeychainProxy reply hander. */
    CloudKeychainReplyBlock replyBlock = ^(CFDictionaryRef returnedValues, CFErrorRef error)
    {
        /* Did we get back some values? */
        if (returnedValues) {
        
            kvs_dict = (returnedValues);
            CFRetain(kvs_dict);
        }
        
        /* Leave the cloud group. */
        dispatch_group_leave(cloud_group);
        
        /* Signal the other queue we're done. */
        dispatch_semaphore_signal(waitSemaphore);
    };

    /* Ask CloudKeychainProxy for all of the raw KVS data. */
    SOSCloudKeychainGetAllObjectsFromCloud(cloud_queue, replyBlock);

    /* Wait for CloudKeychainProxy to respond, up to 30 seconds. */
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    
    /* Release the semaphore. */
	dispatch_release(waitSemaphore);
    
    /* Did we get any raw KVS data from CloudKeychainProxy? */
    if (kvs_dict) {

        dump_dict(log_file, kvs_dict, 0);
    }
    
    /* Dump a footer. */
    fprintf(log_file, "=================\n\n");

    /* Set the result to success. */
    result = SUCCESS;
        
    /* Release the KVS dictionary? */
    if (kvs_dict != NULL) {
    
        CFRelease(kvs_dict);
        kvs_dict = NULL;
    }
    
    return result;
}

static
void
dump_dict(
    FILE *log_file,
    CFDictionaryRef dict,
    const unsigned int indent_level)
{
    struct dict_dump_state dump_state;
    
    /* Setup the context. */
    dump_state.log_file = log_file;
    dump_state.dict = dict;
    dump_state.indent_level = indent_level;

    /* Apply the dumper to each element in the dictionary. */
    CFDictionaryApplyFunction(dict, dump_dict_applier, (void *)&dump_state);
}

static
void
dump_dict_applier(
    const void *key,
    const void *value,
    void *context)
{
    CFTypeRef               key_object;
    CFTypeRef               value_object;
    struct dict_dump_state  *dump_state;
    unsigned int            i;
    char                    buffer[BUFFER_SIZE];
    CFIndex                 length;
    const UInt8*            bytes;
    
    /* Assign the CF types. */
    key_object = (CFTypeRef) key;
    value_object = (CFTypeRef) value;
    
    /* Get the context. */
    dump_state = (struct dict_dump_state *)context;
    
    /* Indent appropriately. */
    for (i = 0; i < dump_state->indent_level; i++) {
    
        fprintf(dump_state->log_file, " ");
    }
    
    /* Determine the key type. */
    if (CFGetTypeID(key_object) == CFStringGetTypeID()) {

        /* Convert the key to a C string. */
        if (!CFStringGetCString((CFStringRef) key_object, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
        
            fprintf(stderr, "Could not convert the key to a C string\n");
            fprintf(dump_state->log_file, "[Failed Key Type]: ");
        } else {
        
            fprintf(dump_state->log_file, "%s: ", buffer);
        }
    }
    
    /* Determine the value type. */
    if (CFGetTypeID(value_object) == CFStringGetTypeID()) {

        /* Convert the value to a C string. */
        if (!CFStringGetCString((CFStringRef) value_object, buffer, BUFFER_SIZE, kCFStringEncodingUTF8)) {
        
            fprintf(stderr, "Could not convert the value to a C string\n");
            fprintf(dump_state->log_file, "[Failed Value Type]: ");
        } else {
        
            fprintf(dump_state->log_file, "%s\n", buffer);
        }
    } else if (CFGetTypeID(value_object) == CFDataGetTypeID()) {
        
        length = CFDataGetLength((CFDataRef)value_object);
        bytes = CFDataGetBytePtr((CFDataRef) value_object);
        
        fprintf(dump_state->log_file, "0x");        

        for (i = 0; i < (unsigned int)length && i < MAX_DATA_RATE; i++) {
        
            fprintf(dump_state->log_file, "%02x", (unsigned char)bytes[i]);
        }
        
        fprintf(dump_state->log_file, " (%ld bytes)\n", length);
        
        
    } else if (CFGetTypeID(value_object) == CFDictionaryGetTypeID()) {
        
        /* Recurse motherfucker! */
        fprintf(dump_state->log_file, "\n");
        dump_dict(dump_state->log_file, (CFDictionaryRef) value_object, dump_state->indent_level + 1);
    } else {
    
        fprintf(dump_state->log_file, "[Unknown Value Type]\n");
    }
}

static
int
dump_asl_sender(
    FILE *log_file,
    const char *asl_sender)
{
    int             result = FAILURE;
    aslmsg          log_query = NULL;
    aslresponse     log_response = NULL;
    aslmsg          log_message;
    char            *message_string;
    uint32_t        message_length;
    
    /*
     * Dump the ASL logs for the given sender.
     */
    
    /* Dump a header. */
    fprintf(log_file, "ASL: %s\n", asl_sender);
    fprintf(log_file, "=================\n");
    
    /* Create the ASL query. */
    log_query = asl_new(ASL_TYPE_QUERY);
    if (log_query == NULL) {
    
        fprintf(stderr, "Could not create ASL query\n");
        goto BAIL;
    }
    
    /* Setup the ASL query. */
    asl_set_query(log_query, ASL_KEY_SENDER, asl_sender, ASL_QUERY_OP_EQUAL);
    
    /* Perform the ASL search. */
    log_response = asl_search(NULL, log_query);
    if (log_response == NULL) {
    
        fprintf(log_file, "Could not perform ASL search for %s\n", asl_sender);
    } else {
    
        /* Enumerate the ASL messages in the response. */
        while ((log_message = aslresponse_next(log_response)) != NULL) {
        
            /* Format the message entry. */
            message_string = asl_format_message((asl_msg_t *)log_message, ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL, ASL_ENCODE_SAFE, &message_length);
            if (message_string == NULL) {
            
                fprintf(stderr, "Could not create ASL message string\n");
                goto BAIL;
            }
            
            fprintf(log_file, "%s", message_string);
            
            /* Release the message string. */
            free(message_string);
            message_string = NULL;        
        }
    }
    
    /* Dump a footer. */
    fprintf(log_file, "=================\n\n");

    /* Set the result to success. */
    result = SUCCESS;
    
BAIL:
    
    /* Release the ASL response? */
    if (log_response != NULL) {
    
        aslresponse_free(log_response);
        log_response = NULL;
    }
    
    /* Release the ASL query? */
    if (log_query != NULL) {
    
        asl_free(log_query);
        log_query = NULL;
    }
    
    return result;
}

static
void
dump_cferror(
    FILE *log_file,
    const char *description,
    CFErrorRef error)
{
    CFStringRef     error_string = NULL;
    char            buffer[BUFFER_SIZE];
    
    error_string = CFErrorCopyDescription(error);
    if (error_string == NULL) {
    
        fprintf(stderr, "Could not copy error description?\n");
        goto BAIL;
    }
    
    (void) CFStringGetCString(error_string, buffer, BUFFER_SIZE, kCFStringEncodingUTF8);
    
    fprintf(stderr, "%s: %s\n", description, buffer);
    fprintf(log_file, "%s: %s\n", description, buffer);

BAIL:

    /* Release the error string? */
    if (error_string != NULL) {
    
        CFRelease(error_string);
        error_string = NULL;
    }
}

#else  // TARGET_IPHONE_SIMULATOR

int
main(
     int argc,
     char **argv)
{
#pragma unused (argc, argv)
    return 0;
}

#endif
