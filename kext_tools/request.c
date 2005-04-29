#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <libc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/bootstrap.h>
#include <mach/kmod.h>

#include "globals.h"
#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/kextmanager_types.h>
#include "paths.h"
#include "request.h"
#include "logging.h"
#include "queue.h"
#include "PTLock.h"

uid_t logged_in_uid = -1;
AuthorizationRef gAuthRef = NULL;

#ifndef NO_CFUserNotification

CFMutableArrayRef gPendedNonsecureKextPaths = NULL;  // must release
CFMutableDictionaryRef gNotifiedNonsecureKextPaths = NULL;  // must release
CFRunLoopSourceRef gCurrentNotificationRunLoopSource = NULL;   // must release
CFUserNotificationRef gCurrentNotification = NULL;   // must release

#endif /* NO_CFUserNotification */

static KXKextManagerError __kextd_load_kext(KXKextRef theKext,
    const char * kmod_name);
#ifndef NO_CFUserNotification
extern void kextd_clear_all_notifications(void);
void kextd_check_notification_queue(void * info);
void kextd_handle_finished_notification(CFUserNotificationRef userNotification,
    CFOptionFlags responseFlags);
static void _kextd_raise_security_notification(CFStringRef kextPath);
#endif /* NO_CFUserNotification */

extern char * CFURLCopyCString(CFURLRef anURL);
extern const char * _KXKextCopyCanonicalPathnameAsCString(KXKextRef aKext);
extern KXKextManagerError _KXKextMakeSecure(KXKextRef aKext);
extern KXKextManagerError _KXKextRaiseSecurityAlert(KXKextRef aKext, uid_t euid);
extern KXKextManagerError _KXKextManagerPrepareKextForLoading(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    CFMutableArrayRef inauthenticKexts);
extern KXKextManagerError _KXKextManagerLoadKextUsingOptions(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    const char * kernel_file,
    const char * patch_dir,
    const char * symbol_dir,
    IOOptionBits load_options,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses);

// load_options
enum 
{
    kKXKextManagerLoadNone	= false,
    kKXKextManagerLoadKernel	= true,
    kKXKextManagerLoadPrelink	= 2,
    kKXKextManagerLoadKextd	= 3
};

#define	KEXTCACHE_COMMAND	"/usr/sbin/kextcache -Flrc"

/*******************************************************************************
*
*******************************************************************************/
Boolean kextd_launch_kernel_request_thread(void)
{
    Boolean result = true;
    pthread_attr_t kernel_request_thread_attr;
    pthread_t      kernel_request_thread;

    queue_init(&g_request_queue);

    gKernelRequestQueueLock = PTLockCreate();
    if (!gKernelRequestQueueLock) {
        kextd_error_log("failed to create kernel request queue lock");
        result = false;
        goto finish;
    }

    pthread_attr_init(&kernel_request_thread_attr);
    pthread_create(&kernel_request_thread,
        &kernel_request_thread_attr,
        kextd_kernel_request_loop, NULL);
    pthread_detach(kernel_request_thread);

finish:
    if (!result) {
        if (gKernelRequestQueueLock) {
            PTLockFree(gKernelRequestQueueLock);
        }
        if (gRunLoopSourceLock) {
            PTLockFree(gRunLoopSourceLock);
        }
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void * kextd_kernel_request_loop(void * arg)
{
    kmod_args_t data = 0;         // must vm_deallocate()'d
    mach_msg_type_number_t data_count = 0;
    char * kmod_name = NULL;  // don't free; that's done by the consumer
    mach_port_t host_port = PORT_NULL;

    host_port = mach_host_self(); /* must be privileged to work */
    if (!MACH_PORT_VALID(host_port)) {
        // FIXME: Put something here
    }

    while (1) {
        kern_return_t kern_result;
        kmod_load_extension_cmd_t * request;
        unsigned int request_type;

       /* Clean up kernel-allocated data at top of loop to handle error
        * conditions that cause a continue.
        */
        if (data) {
            kern_result = vm_deallocate(mach_task_self(),
                (vm_address_t)data, data_count);
            if (kern_result != KERN_SUCCESS) {
                kextd_error_log("vm_deallocate() failed; aborting");
                // FIXME: Is this really necessary?
                exit(1);
            }
            data = 0;
            data_count = 0;
        }

       /* Wait for kernel to unblock this thread with a potential
        * request.
        */
        kern_result = kmod_control(host_port, 0, KMOD_CNTL_GET_CMD,
            &data, &data_count);
        if (kern_result != KERN_SUCCESS) {
            kextd_error_log(
                "kmod_control() error # %d; aborting kernel request loop",
                kern_result);
            goto finish;
        }

        request = (kmod_load_extension_cmd_t *)data;
        request_type = request->type;


       /* Examine the potential request.
        */
        switch (request_type) {

          case kIOCatalogMatchIdle:
            // nothing to do
            // could use this to dump unneeded memory
            break;

          case KMOD_LOAD_EXTENSION_PACKET:
            kmod_name = strdup(request->name);
            if (!kmod_name) {
                kextd_error_log(
                    "failed to read kmod name from kernel request");
                continue;
            }
            break;

          default:
                kextd_error_log(
                    "received invalid kernel request, type %d",
                    request_type);
                continue;
            break;
        }

       /* If we have a kmod name then we have a load request. Kick the
        * name over to the main thread via the run loop.
        */
        if (kmod_name) {
            request_t * load_request;

            load_request = (request_t *)malloc(sizeof(request_t));
            if (!load_request) {
                kextd_error_log(
                    "failed to allocate data for kernel request");
            } else {
                memset(load_request, 0, sizeof(request_t));
                load_request->type = load_request->type;
                load_request->kmodname = kmod_name;
                // queue up a reqest
                PTLockTakeLock(gKernelRequestQueueLock);
                queue_enter(&g_request_queue, load_request, request_t *, link);
                PTLockUnlock(gKernelRequestQueueLock);

                // wake up the runloop
                PTLockTakeLock(gRunLoopSourceLock);
                CFRunLoopSourceSignal(gKernelRequestRunLoopSource);
                CFRunLoopWakeUp(gMainRunLoop);
                PTLockUnlock(gRunLoopSourceLock);
            }
        }
    }

finish:

   /* Dispose of the host port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (PORT_NULL != host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }

    pthread_exit(NULL);
    return NULL;
}

/*******************************************************************************
* 
*******************************************************************************/
static int load_request_equal(request_t * a, request_t * b)
{
    if (a->type != b->type) return 0;
    if (strcmp(a->kmodname, b->kmodname)) return 0;
    return 1;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked from within kextd_kernel_request_loop().
*******************************************************************************/
void kextd_handle_kernel_request(void * info)
{
    PTLockTakeLock(gKernelRequestQueueLock);

    while (!queue_empty(&g_request_queue)) {
        request_t * load_request = NULL;       // must free
        request_t * this_load_request = NULL;  // free if duplicate
        unsigned int type;
        char * kmod_name = NULL; // must release

        load_request = (request_t *)queue_first(&g_request_queue);
        queue_remove(&g_request_queue, load_request, request_t *, link);

       /*****
        * Scan the request queue for duplicates of the first one and
        * pull them out.
        */
        this_load_request = (request_t *)queue_first(&g_request_queue);
        while (!queue_end((request_t *)&g_request_queue, this_load_request)) {
            request_t * next_load_request = NULL; // don't free
            next_load_request = (request_t *)
                queue_next(&this_load_request->link);

            if (load_request_equal(load_request, this_load_request)) {
                queue_remove(&g_request_queue, this_load_request,
                    request_t *, link);
                free(this_load_request->kmodname);
                free(this_load_request);
            }
            this_load_request = next_load_request;
        }

        PTLockUnlock(gKernelRequestQueueLock);

        type = load_request->type;
        kmod_name = load_request->kmodname;

        free(load_request);

        if (kmod_name) {
	    KXKextManagerError load_result;
	    static boolean_t have_signalled_load = FALSE;

            kextd_load_kext(kmod_name, &load_result);
            free(kmod_name);

	    if ((load_result == kKXKextManagerErrorNone ||
		    load_result == kKXKextManagerErrorAlreadyLoaded)
		    && !have_signalled_load
		    && (getppid() > 1)) {
		// ppid == 1 => parent is no longer waiting
		have_signalled_load = TRUE;
		int ret;
		if (g_verbose_level >= 1) {
		    kextd_log("running kextcache");
		}
		ret = system(KEXTCACHE_COMMAND);
		if (ret != 0) {
		    kextd_error_log("kextcache exec(%d)", ret);
		}
	    }
        }

        PTLockTakeLock(gKernelRequestQueueLock);
    }

    PTLockUnlock(gKernelRequestQueueLock);
    return;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a kernel or client request.
*******************************************************************************/
void kextd_load_kext(char * kmod_name,
    KXKextManagerError * kext_result /* out */)
{
    CFStringRef kextID = NULL; // must release
    KXKextRef theKext = NULL;  // do not release
    KXKextManagerError load_result = kKXKextManagerErrorNone;

    kextID = CFStringCreateWithCString(kCFAllocatorDefault, kmod_name,
        kCFStringEncodingMacRoman);

    if (!kextID) {
        // FIXME: Log no-memory error? Exit?
        return;
    }

    if (g_verbose_level > 0) {
        kextd_log("kernel requests extension with id %s", kmod_name);
    }

   /*****
    * Find the kext to load based on its ID. If we don't find this
    * kext, we can not satisfy the load request, so remove from the
    * kernel's IOCatalog all personalities that reference the
    * requested ID.
    */
    theKext = KXKextManagerGetKextWithIdentifier(gKextManager, kextID);
    if (!theKext) {
        KXKextManagerError remove_result;
        CFDictionaryRef personality = NULL;  // must release
        const void * keys[1];
        const void * values[1];

        if (kext_result) {
            *kext_result = kKXKextManagerErrorKextNotFound;
        }

        kextd_error_log("can't find extension with id %s",
                  kmod_name);

        keys[0] = CFSTR("CFBundleIdentifier");
        values[0] = kextID;
        personality = CFDictionaryCreate(kCFAllocatorDefault,
            keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!personality) {
            kextd_error_log("out of memory");
            goto finish;
        }

        remove_result = KXKextManagerRemovePersonalitiesFromCatalog(
            gKextManager, personality);

        CFRelease(personality);

        if (remove_result != kKXKextManagerErrorNone) {
            kextd_error_log("failed to remove personalities from IOCatalogue");
        }
        goto finish;
    }

    load_result = __kextd_load_kext(theKext, kmod_name);
    if (kext_result) {
        *kext_result = load_result;
    }

finish:
    if (kextID) CFRelease(kextID);

    return;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a kernel or client request.
*******************************************************************************/
static KXKextManagerError __kextd_load_kext(KXKextRef theKext,
    const char * kmod_name)
{
    KXKextManagerError load_result = kKXKextManagerErrorNone;
    kern_return_t kern_result;
    CFMutableArrayRef inauthenticKexts = NULL;  // must release
#ifndef NO_CFUserNotification
    CFIndex inauthentic_kext_count = 0;
    CFIndex k = 0;
#endif /* NO_CFUserNotification */

    inauthenticKexts = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!inauthenticKexts) {
        load_result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    load_result = _KXKextManagerPrepareKextForLoading(
        gKextManager, theKext, NULL /* kext_name */,
        false /* check loaded for dependencies */, true /* do_load */,
        inauthenticKexts);

    if (load_result == kKXKextManagerErrorCache) {
        kextd_error_log("scheduling rescan of all kexts due to cache "
            "inconsistency");
        // do not take the runloop source lock here, we're in the same thread
        CFRunLoopSourceSignal(gRescanRunLoopSource);
        CFRunLoopWakeUp(gMainRunLoop);
        goto finish;

    } else if (load_result == kKXKextManagerErrorAlreadyLoaded ||
        load_result == kKXKextManagerErrorLoadedVersionDiffers) {

        goto post_load;

#ifndef NO_CFUserNotification

    } else if (load_result == kKXKextManagerErrorAuthentication) {

        inauthentic_kext_count = CFArrayGetCount(inauthenticKexts);
        if (inauthentic_kext_count) {
            for (k = 0; k < inauthentic_kext_count; k++) {
                KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                    inauthenticKexts, k);
                CFStringRef kextPath = NULL; // must release
                kextPath = KXKextCopyAbsolutePath(thisKext);
                if (!kextPath) {
                    load_result = kKXKextManagerErrorNoMemory;
                    goto finish;
                }
                if (!CFDictionaryGetValue(gNotifiedNonsecureKextPaths, kextPath)) {
                    CFArrayAppendValue(gPendedNonsecureKextPaths, kextPath);
                    CFDictionarySetValue(gNotifiedNonsecureKextPaths, kextPath,
                        kCFBooleanTrue);
                }
                if (kextPath) {
                    CFRelease(kextPath);
                    kextPath = NULL;
                }
            }
        }

        if (logged_in_uid != -1) {
            CFRunLoopSourceSignal(gNotificationQueueRunLoopSource);
            CFRunLoopWakeUp(gMainRunLoop);
        }
        goto post_load;

#endif /* NO_CFUserNotification */

    } else if (load_result != kKXKextManagerErrorNone) {
        goto post_load;
    }

    load_result = _KXKextManagerLoadKextUsingOptions(
        gKextManager,
        theKext,
        NULL,  // kext name as C string; optional
        g_kernel_file, // kernel file
        g_patch_dir,
        g_symbol_dir,
        kKXKextManagerLoadKextd,     // load_options
        true,  // call the start routine
        false, // not interactive; don't confirm each load stage
        false, // not interactive; don't ask to overwrite symbols
        gOverwrite_symbols,
        false, // get addresses from kernel
        0,     // num addresses
        NULL); // load addresses for symbol gen.

post_load:

   /*****
    * On successful load, notify IOCatalog.
    */
    if (load_result == kKXKextManagerErrorNone ||
        load_result == kKXKextManagerErrorAlreadyLoaded) {

        kern_result = IOCatalogueModuleLoaded(g_io_master_port,
            (char *)kmod_name);
        if (kern_result != KERN_SUCCESS) {
            kextd_error_log("failed to notify IOCatalogue that %s loaded",
                kmod_name);
        }

        goto finish;
    }

   /*****
    * On failed load, remove kext's pesonalities from the IOCatalog.
    */
    if (load_result != kKXKextManagerErrorNone &&
        load_result != kKXKextManagerErrorAlreadyLoaded &&
        load_result != kKXKextManagerErrorLoadedVersionDiffers) {

        KXKextManagerRemoveKextPersonalitiesFromCatalog(
            gKextManager, theKext);
        goto finish;
    }

finish:
    if (inauthenticKexts) CFRelease(inauthenticKexts);
    return load_result;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t _kextmanager_path_for_bundle_id(
    mach_port_t server,
    kext_bundle_id_t bundle_id,
    posix_path_t path,
    KXKextManagerError * kext_result)
{
    kern_return_t result = KERN_SUCCESS;
    KXKextManagerError kmResult = kKXKextManagerErrorNone;

    CFStringRef  kextID = NULL;    // must release
    KXKextRef    theKext = NULL;   // don't release
    CFURLRef     kextURL = NULL;   // must release
    CFStringRef  kextPath = NULL;  // must release
    char *       kext_path = NULL; // must free

    path[0] = '\0';
    
    if (g_verbose_level >= 1) {
        kextd_log("received client request for path to bundle %s", bundle_id);
    }

    kextID = CFStringCreateWithCString(kCFAllocatorDefault, bundle_id,
        kCFStringEncodingMacRoman);
    if (!kextID) {
        kmResult = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    theKext = KXKextManagerGetLoadedOrLatestKextWithIdentifier(
        gKextManager, kextID);
    if (!theKext) {
        if (g_verbose_level >= 1) {
            kextd_log("bundle %s not found", bundle_id);
        }
        kmResult = kKXKextManagerErrorKextNotFound;
        goto finish;
    }

    kextURL = KXKextGetAbsoluteURL(theKext);
    if (!kextURL) {
        kmResult = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    kext_path = CFURLCopyCString(kextURL);
    if (!kext_path) {
        kmResult = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    // FIXME: Need to make sure we bounds-check the out parameter
    // FIXME: ...for the path we got.
    strcpy(path, kext_path);

    if (g_verbose_level >= 1) {
        kextd_log("returning bundle path %s", path);
    }

finish:
    if (kextID)    CFRelease(kextID);
    if (kextPath)  CFRelease(kextPath);
    if (kext_path) free(kext_path);
    if (kext_result) {
        *kext_result = kmResult;
    }

    gClientUID = -1;

    return result;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t kext_load_bundle_with_id(
    mach_port_t server,
    char * bundle_id,
    KXKextManagerError * kext_result)
{
    kern_return_t result = KERN_FAILURE;

    // Do not implement this until we determine a security check.
    if (kext_result) {
        *kext_result = kKXKextManagerErrorUnspecified;
    }
    goto finish;

    kextd_load_kext(bundle_id, kext_result);
    result = KERN_SUCCESS;

finish:
    gClientUID = -1;
    return result;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
extern CFArrayRef _KXKextRepositoryGetCandidateKexts(
    KXKextRepositoryRef aRepository);

kern_return_t _kextmanager_create_property_value_array(
    mach_port_t server,
    char * property_key,
    char ** xml_data_out,
    int * xml_data_length)

{
    kern_return_t result = KERN_SUCCESS;

    CFStringRef  propertyKey = NULL;          // must release

    CFMutableArrayRef propertyValues = NULL;  // must release
    CFDictionaryRef   infoDictionary = NULL;  // don't release
    CFTypeRef         value = NULL;           // don't release

    CFArrayRef repositories = NULL;  // don't release
    CFIndex numRepositories, i;
    CFArrayRef candidateKexts = NULL;
    CFMutableDictionaryRef newDict = NULL;  // must release
    CFStringRef kextPath = NULL;         // must release
    CFStringRef kextVersion = NULL;      // do not release
    
    CFDataRef    xmlData = NULL;      // must release

    if (g_verbose_level >= 1) {
        kextd_log("received client request for property value array");
    }

    if (!xml_data_out || !xml_data_length) {
        result = KERN_INVALID_ARGUMENT;
        goto finish;
    }

    propertyKey = CFStringCreateWithCString(kCFAllocatorDefault, property_key,
        kCFStringEncodingMacRoman);
    if (!propertyKey) {
        result = KERN_FAILURE;
        goto finish;
    }

    propertyValues = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!propertyValues) {
        result = KERN_FAILURE;
        goto finish;
    }

    repositories = KXKextManagerGetRepositories(gKextManager);
    numRepositories = CFArrayGetCount(repositories);

    for (i = 0; i < numRepositories; i++) {
        CFIndex numKexts, k;
        KXKextRepositoryRef thisRepository =
           (KXKextRepositoryRef)CFArrayGetValueAtIndex(
           repositories, i);

        candidateKexts = _KXKextRepositoryGetCandidateKexts(thisRepository);
        numKexts = CFArrayGetCount(candidateKexts);

        for (k = 0; k < numKexts; k++) {
            KXKextRef thisKext =
                (KXKextRef)CFArrayGetValueAtIndex(candidateKexts, k);

            // skip kexts known to be unloadable
            if (!KXKextIsValid(thisKext)) {
                continue;
            }
            //??? if (KXKextGetLoadFailed(thisKext)) continue;
            if (KXKextManagerGetSafeBootMode(gKextManager) &&
                !KXKextIsEligibleDuringSafeBoot(thisKext)) {
                continue;
            }
            if (!KXKextIsEnabled(thisKext)) {
                continue;
            }

            infoDictionary = KXKextGetInfoDictionary(thisKext);
            if (!infoDictionary) {
                continue;
            }

            value = CFDictionaryGetValue(infoDictionary, propertyKey);
            if (!value) {
                continue;
            }

            newDict = CFDictionaryCreateMutable(
                kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
            if (!newDict) {
                result = KERN_FAILURE;
                goto finish;
            }

            CFDictionarySetValue(newDict, CFSTR("Data"), value);

            CFDictionarySetValue(newDict, CFSTR("CFBundleIdentifier"),
                KXKextGetBundleIdentifier(thisKext));

            kextPath = KXKextCopyAbsolutePath(thisKext);
            if (!kextPath) {
                result = KERN_FAILURE;
                goto finish;
            }
            CFDictionarySetValue(newDict, CFSTR("OSBundlePath"), kextPath);
            CFRelease(kextPath);
            kextPath = NULL;

            kextVersion = CFDictionaryGetValue(infoDictionary,
                CFSTR("CFBundleVersion"));
            if (!kextVersion) {
                result = KERN_FAILURE;
                goto finish;
            }
            CFDictionarySetValue(newDict, CFSTR("CFBundleVersion"),
                kextVersion);
            // do not release
            kextVersion = NULL;

            CFArrayAppendValue(propertyValues, newDict);
            CFRelease(newDict);
            newDict = NULL;
        }
    }

    xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
         propertyValues);
    if (!xmlData) {
        result = KERN_FAILURE;
        goto finish;
    }

    *xml_data_length = (int)CFDataGetLength(xmlData);

    result = vm_allocate(mach_task_self(), (vm_address_t *)xml_data_out,
        *xml_data_length, VM_FLAGS_ANYWHERE);
    if (result != KERN_SUCCESS) {
        // FIXME: Log a message here?
        goto finish;
    }

    memcpy(*xml_data_out, CFDataGetBytePtr(xmlData), *xml_data_length);

finish:
    if (propertyKey)    CFRelease(propertyKey);
    if (propertyValues) CFRelease(propertyValues);
    if (newDict)        CFRelease(newDict);
    if (kextPath)       CFRelease(kextPath);
    if (xmlData)        CFRelease(xmlData);

    gClientUID = -1;

    return result;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t _kextmanager_user_did_log_in(
    mach_port_t server,
    int euid,
    AuthorizationExternalForm authref)
{
    kern_return_t result = KERN_SUCCESS;

    logged_in_uid = euid;

#ifndef NO_CFUserNotification
    CFRunLoopSourceSignal(gNotificationQueueRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);
#endif NO_CFUserNotification

//finish:

    gClientUID = -1;

    return result;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t _kextmanager_user_will_log_out(
    mach_port_t server,
    int euid)
{
    kern_return_t result = KERN_SUCCESS;

#ifndef NO_CFUserNotification
    kextd_clear_all_notifications();
#endif NO_CFUserNotification

    logged_in_uid = -1;
    gClientUID = -1;

    return result;
}

#ifndef NO_CFUserNotification

/*******************************************************************************
*
*******************************************************************************/
void kextd_check_notification_queue(void * info)
{
    CFStringRef kextPath = NULL;  // do not release

    if (logged_in_uid == -1) {
        return;
    }

    if (gCurrentNotificationRunLoopSource) {
        return;
    }

    if (CFArrayGetCount(gPendedNonsecureKextPaths)) {
        kextPath = (CFStringRef)CFArrayGetValueAtIndex(gPendedNonsecureKextPaths, 0);
        _kextd_raise_security_notification(kextPath);
        CFArrayRemoveValueAtIndex(gPendedNonsecureKextPaths, 0);
    }

//finish:

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_handle_finished_notification(CFUserNotificationRef userNotification,
    CFOptionFlags responseFlags)
{

    if (gCurrentNotification) {
        CFRelease(gCurrentNotification);
        gCurrentNotification = NULL;
    }

    if (gCurrentNotificationRunLoopSource) {
        CFRunLoopRemoveSource(gMainRunLoop, gCurrentNotificationRunLoopSource,
            kCFRunLoopDefaultMode);
        CFRelease(gCurrentNotificationRunLoopSource);
        gCurrentNotificationRunLoopSource = NULL;
    }

    CFRunLoopSourceSignal(gNotificationQueueRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void _kextd_raise_security_notification(CFStringRef kextPath)
{
    CFMutableDictionaryRef alertDict = NULL;  // must release
    CFMutableArrayRef alertMessageArray = NULL; // must release
    CFURLRef iokitFrameworkBundleURL = NULL;  // must release
    SInt32 userNotificationError = 0;

    alertDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!alertDict) {
        goto finish;
    }

    iokitFrameworkBundleURL = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/Frameworks/IOKit.framework"),
        kCFURLPOSIXPathStyle, true);
    if (!iokitFrameworkBundleURL) {
        goto finish;
    }

    alertMessageArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!alertMessageArray) {
        goto finish;
    }

   /* This is the localized format string for the alert message.
    */
    CFArrayAppendValue(alertMessageArray,
        CFSTR("The system extension \""));
    CFArrayAppendValue(alertMessageArray, kextPath);
    CFArrayAppendValue(alertMessageArray,
        CFSTR("\" was installed improperly and cannot be used. "
              "Please try reinstalling it, or contact the product's vendor "
              "for an update."));

    CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
        iokitFrameworkBundleURL);
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
        CFSTR("System extension cannot be used."));
    CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
        CFSTR("OK"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
        alertMessageArray);

    gCurrentNotification = CFUserNotificationCreate(kCFAllocatorDefault,
        0 /* time interval */, kCFUserNotificationCautionAlertLevel,
        &userNotificationError, alertDict);
    if (!gCurrentNotification) {
        kextd_error_log(
            "error creating user notification (%d)", userNotificationError);
        goto finish;
    }

     gCurrentNotificationRunLoopSource = CFUserNotificationCreateRunLoopSource(
         kCFAllocatorDefault, gCurrentNotification,
         &kextd_handle_finished_notification, 5 /* FIXME: cheesy! */);
    if (!gCurrentNotificationRunLoopSource) {
        CFRelease(gCurrentNotification);
        gCurrentNotification = NULL;
    }
    CFRunLoopAddSource(gMainRunLoop, gCurrentNotificationRunLoopSource,
        kCFRunLoopDefaultMode);

finish:

    if (alertDict)               CFRelease(alertDict);
    if (alertMessageArray)       CFRelease(alertMessageArray);
    if (iokitFrameworkBundleURL) CFRelease(iokitFrameworkBundleURL);

    return;
}

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t _kextmanager_get_logged_in_userid(
    mach_port_t server,
    int * euid)
{
    kern_return_t result = KERN_SUCCESS;

    *euid = logged_in_uid;

    gClientUID = -1;

    return result;
}

#endif /* NO_CFUserNotification */

/*******************************************************************************
*
*******************************************************************************/
kern_return_t _kextmanager_record_nonsecure_kextload(
    mach_port_t server,
    char * load_data,
    int load_data_length)
{
    kern_return_t result = KERN_SUCCESS;
#ifndef NO_CFUserNotification
    CFStringRef kextPath = NULL; // must release

    kextPath = CFStringCreateWithCString(kCFAllocatorDefault, load_data,
        kCFStringEncodingMacRoman);
    if (!kextPath) {
        result = KERN_FAILURE;
        goto finish;
    }

    if (!CFDictionaryGetValue(gNotifiedNonsecureKextPaths, kextPath)) {
        CFArrayAppendValue(gPendedNonsecureKextPaths, kextPath);
        CFDictionarySetValue(gNotifiedNonsecureKextPaths, kextPath,
            kCFBooleanTrue);
    }

    if (logged_in_uid != -1) {
        CFRunLoopSourceSignal(gNotificationQueueRunLoopSource);
        CFRunLoopWakeUp(gMainRunLoop);
    }

#else
    result = KERN_FAILURE;
#endif /* NO_CFUserNotification */


finish:

#ifndef NO_CFUserNotification
    if (kextPath) CFRelease(kextPath);
#endif /* NO_CFUserNotification */
    gClientUID = -1;
    return result;
}
