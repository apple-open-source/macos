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

CFMutableArrayRef gPendedNonsecureKexts = NULL;  // must release
CFMutableArrayRef gPendedKextloadOperations = NULL;  // must release
CFMutableArrayRef gScheduledNonsecureKexts = NULL;  // must release

pid_t gFork_pid = -1;

CFUserNotificationRef gSecurityNotification = NULL;  // must release
CFUserNotificationRef gFailureNotification = NULL;  // must release
KXKextRef gSecurityAlertKext = NULL;        // don't release
Boolean gResendSecurityAlertKextPersonalities = false;
CFOptionFlags gSecurityAlertResponse = 0;
CFOptionFlags gFailureAlertResponse = 0;
PTLockRef gUserAuthorizationLock = NULL;               // must release
Boolean gWaitingForAuthorization = false;
Boolean gAuthorizationReady = false;
OSStatus auth_result = errAuthorizationSuccess;

#endif /* NO_CFUserNotification */

extern char ** environ;

static KXKextManagerError __kextd_load_kext(KXKextRef theKext,
    const char * kmod_name);
#ifndef NO_CFUserNotification
static void _kextd_poll_alert_response(void);
static void _kextd_raise_security_alert(KXKextRef aKext);
static void _kextd_raise_failure_alert(KXKextRef aKext);
void _kextd_launch_authorization(void);
void * _kextd_authorize_user(void * arg);
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
    Boolean do_load,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses);

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

    gRunLoopSourceLock = PTLockCreate();
    if (!gRunLoopSourceLock) {
        kextd_error_log(
            "failed to create kernel request run loop source lock");
        result = false;
        goto finish;
    }

    gUserAuthorizationLock = PTLockCreate();
    if (!gUserAuthorizationLock) {
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
        if (gUserAuthorizationLock) {
            PTLockFree(gUserAuthorizationLock);
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
            kextd_load_kext(kmod_name, NULL);
            free(kmod_name);
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
        true /* check loaded for dependencies */, true /* do_load */,
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

        if (logged_in_uid == -1) {
            CFIndex count, i;
            Boolean addIt = true;

            count = CFArrayGetCount(gPendedNonsecureKexts);
            for (i = 0; i < count; i++) {
                KXKextRef scanKext = (KXKextRef)
                    CFArrayGetValueAtIndex(gPendedNonsecureKexts, i);

                if (scanKext == theKext) {
                    addIt = false;
                    break;
                }
            }
            if (addIt) {
                CFArrayAppendValue(gPendedNonsecureKexts, theKext);
            }
            goto post_load;

        } else {

            inauthentic_kext_count = CFArrayGetCount(inauthenticKexts);
            if (inauthentic_kext_count) {
                for (k = 0; k < inauthentic_kext_count; k++) {
                    KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                        inauthenticKexts, k);
                    CFArrayAppendValue(gScheduledNonsecureKexts, thisKext);
                }
                PTLockTakeLock(gRunLoopSourceLock);
                CFRunLoopSourceSignal(gNonsecureKextRunLoopSource);
                CFRunLoopWakeUp(gMainRunLoop);
                PTLockUnlock(gRunLoopSourceLock);
                goto post_load;
            }
        }

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
        true,  // do load
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
*
*******************************************************************************/
#ifndef NO_CFUserNotification

void kextd_resend_nonsecure_personalities(void)
{
    CFMutableArrayRef allKextPersonalities = NULL;  // must release
    CFArrayRef thisKextPersonalities = NULL;        // must release
    CFIndex inauthentic_kext_count = 0;
    CFIndex k;

   /* First send personalities down for all pended nonsecure kexts.
    */
    allKextPersonalities = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!allKextPersonalities) {
        goto finish;
    }

    inauthentic_kext_count = CFArrayGetCount(gPendedNonsecureKexts);
    if (inauthentic_kext_count && logged_in_uid != -1) {

        for (k = 0; k < inauthentic_kext_count; k++) {
            KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                gPendedNonsecureKexts, k);

            if (thisKextPersonalities) {
                CFRelease(thisKextPersonalities);
                thisKextPersonalities = NULL;
            }

            thisKextPersonalities = KXKextCopyPersonalitiesArray(thisKext);
            if (!thisKextPersonalities) {
                goto finish;
            }

            CFArrayAppendArray(allKextPersonalities, thisKextPersonalities,
                CFRangeMake(0, CFArrayGetCount(thisKextPersonalities)));
            KXKextManagerRequalifyKext(gKextManager, thisKext);
        }
    }

    CFArrayRemoveAllValues(gPendedNonsecureKexts);

    if (KXKextManagerSendPersonalitiesToCatalog(gKextManager,
           allKextPersonalities) != kKXKextManagerErrorNone) {

        kextd_error_log("can't send kext personalities to kernel");
        goto finish;
    }

   /* Now schedule all pended nonsecure kext load operations to be redone.
    */
    PTLockTakeLock(gRunLoopSourceLock);
    CFRunLoopSourceSignal(gNonsecureKextRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);
    PTLockUnlock(gRunLoopSourceLock);

finish:

    if (thisKextPersonalities) CFRelease(thisKextPersonalities);
    if (allKextPersonalities)  CFRelease(allKextPersonalities);

    return;
}

#endif /* NO_CFUserNotification */

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

    if (AuthorizationCreateFromExternalForm(&authref,
	    &gAuthRef) != errAuthorizationSuccess) {

        result = KERN_FAILURE;
        goto finish;
    }

#ifndef NO_CFUserNotification
    kextd_resend_nonsecure_personalities();
#endif /* NO_CFUserNotification */

finish:

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

   /* Drop any panels awaiting user input.
    */
    if (gSecurityNotification) {
        CFUserNotificationCancel(gSecurityNotification);
        CFRelease(gSecurityNotification);
        gSecurityNotification = NULL;
    }
    if (gFailureNotification) {
        CFUserNotificationCancel(gFailureNotification);
        CFRelease(gFailureNotification);
        gFailureNotification = NULL;
    }
    gSecurityAlertKext = NULL;
    gResendSecurityAlertKextPersonalities = false;

    PTLockTakeLock(gUserAuthorizationLock);
    gWaitingForAuthorization = false;
    gAuthorizationReady = false;
    PTLockUnlock(gUserAuthorizationLock);

    if (gAuthRef) {
        AuthorizationFree(gAuthRef, 0);
        gAuthRef = NULL;
    }
    logged_in_uid = -1;
    gClientUID = -1;

    return result;
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

/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
#ifndef NO_CFUserNotification

void kextd_handle_pended_kextload(void * info)
{
    CFDictionaryRef loadDataDict = NULL;  // must release
    CFDataRef loadData = NULL; // must release
    CFDataRef dataValue = NULL;   // scratch value, don't release
    const char * data_value = NULL;     // scratch value, don't free
    CFArrayRef argvArray = NULL;  // don't release, from loadDataDict
    CFStringRef errorString = NULL;  // must release
    Boolean scheduleAnother = false;
    Boolean waitingForAuthorization = false;
    Boolean authorizationReady = false;
    FILE *ext_auth_mbox = NULL;
    char *kextd_ext_auth_env = NULL;

    PTLockTakeLock(gUserAuthorizationLock);
    waitingForAuthorization = gWaitingForAuthorization;
    authorizationReady = gAuthorizationReady;
    PTLockUnlock(gUserAuthorizationLock);

    if (gFork_pid != -1) {

        pid_t wait_pid;
        int status = 0;  // always clear status before calling wait!

        wait_pid = waitpid(gFork_pid, &status, WUNTRACED | WNOHANG);
        if (wait_pid) {
            gFork_pid = -1;

            if (WIFEXITED(status)) {
                /* do nothing, just checking it's ok */
            } else if (WIFSIGNALED(status)) {
                kextd_error_log("forked kextload task exited by signal (%d)",
                    WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                kextd_error_log("forked kextload load task has stopped");
            } else {
                kextd_error_log("unknown result from forked kextload task");
            }
        }

       /* Ping the pended kextload run loop source in case there are
        * any more ops.
        */
        sleep(1);  // don't spin too fast
        scheduleAnother = true;

    } else if (logged_in_uid != -1 && waitingForAuthorization) {

       /* Wait for a pending authorization doing anything following.
        */
        sleep(1);  // don't spin too fast
        scheduleAnother = true;

    } else if (logged_in_uid != -1 && (authorizationReady || 
        gSecurityNotification || gFailureNotification) ) {

       /* Wait for an a finished authorization or pending alert to complete
        * before running kextload, which puts up the same authorization/alert.
        */
        _kextd_poll_alert_response();
        scheduleAnother = true;

    } else if (gSecurityAlertKext && gResendSecurityAlertKextPersonalities) {

        KXKextManagerSendKextPersonalitiesToCatalog(
            gKextManager, gSecurityAlertKext, NULL, false /* interactive */,
            KXKextManagerGetSafeBootMode(gKextManager));

        gSecurityAlertKext = NULL;
        gResendSecurityAlertKextPersonalities = false;
        scheduleAnother = true;

    } else  if (CFArrayGetCount(gScheduledNonsecureKexts) && logged_in_uid != -1) {

        KXKextRef thisKext = NULL;  // don't release
        thisKext = (KXKextRef)CFArrayGetValueAtIndex(gScheduledNonsecureKexts, 0);
        if (!thisKext) {
            scheduleAnother = true;
            goto finish;
        }
        CFArrayRemoveValueAtIndex(gScheduledNonsecureKexts, 0);

       /* The kext might have been fixed in a prior go through the run loop.
        */
        if (KXKextIsAuthentic(thisKext)) {
            scheduleAnother = true;
            goto finish;
        }
        _kextd_raise_security_alert(thisKext);
        scheduleAnother = true;

    } else if (CFArrayGetCount(gPendedKextloadOperations) && logged_in_uid != -1) {

        const char * working_dir = NULL;  // don't free
        int k_argc = 0;
        AuthorizationExternalForm auth_ext_form;

       /* Handle any single pended load from kextload.
        */
        loadData = (CFDataRef)CFArrayGetValueAtIndex(
            gPendedKextloadOperations, 0);
        if (loadData) {
            CFRetain(loadData);
            CFArrayRemoveValueAtIndex(gPendedKextloadOperations, 0);
        } else {
            kextd_error_log("can't get pended kextload operation data");
            scheduleAnother = true;
            goto finish;
        }

        gFork_pid = -1;

        loadDataDict = CFPropertyListCreateFromXMLData(
            kCFAllocatorDefault, loadData,
            kCFPropertyListImmutable, &errorString);
        if (!loadDataDict) {
            if (errorString) {
                CFIndex length = CFStringGetLength(errorString);
                char * error_string = (char *)malloc((1+length) * sizeof(char));
                if (!error_string) {
                    goto finish;
                }
                if (CFStringGetCString(errorString, error_string,
                    length, kCFStringEncodingMacRoman)) {
                    kextd_error_log("error reading kextload operation data: %s",
                        error_string);
                } else {
                    kextd_error_log(
                        "unknown error reading kextload operation data");
                }
                free(error_string);
            }
            goto finish;
        }

        dataValue = (CFDataRef)CFDictionaryGetValue(loadDataDict,
            CFSTR("workingDir"));
        if (!dataValue) {
            goto finish;
        }
        working_dir = CFDataGetBytePtr(dataValue);
        // don't release working_dir

        argvArray = (CFArrayRef)CFDictionaryGetValue(loadDataDict,
            CFSTR("argv"));
        if (!argvArray) {
            goto finish;
        }
        k_argc = CFArrayGetCount(argvArray);
        if (k_argc <= 1) {
            goto finish;
        }
        
        if (errAuthorizationSuccess == AuthorizationMakeExternalForm(gAuthRef, &auth_ext_form))
        {
            do {
                ext_auth_mbox = tmpfile();
                if (!ext_auth_mbox)
                    break;
                if (fwrite(&auth_ext_form, sizeof(auth_ext_form), 1, ext_auth_mbox) != 1)
                {
                    fclose(ext_auth_mbox); 
                    break;
                }
                fflush(ext_auth_mbox);
                asprintf(&kextd_ext_auth_env, "KEXTD_AUTHORIZATION=%d", fileno(ext_auth_mbox));
            } while (0);
        }


        gFork_pid = fork();
        if (gFork_pid < 0) {
            kextd_error_log("can't fork child process to run kextload");
            gFork_pid = -1;
            goto finish;
        } else if (gFork_pid == 0) {
            // child
            char ** k_argv = NULL;      // must free
            char kextd_launch[100];     // WARNING: Fixed-length buffer!
            int env_length = 0;
            char ** envp = NULL;
            char ** envp_copy = NULL;
            char ** kextload_env = NULL; // must free
            int i;

           /* Set up argv.
            */
            k_argv = (char **)malloc((1 + k_argc) * sizeof(char *));
            if (!k_argv) {
                kextd_error_log("can't build argv array for kextload");
                exit(-1);
            }

            bzero(k_argv, (1 + k_argc) * sizeof(char *));

            for (i = 0; i < k_argc; i++) {
                dataValue = (CFDataRef)CFArrayGetValueAtIndex(argvArray, i);
                data_value = CFDataGetBytePtr(dataValue);
                k_argv[i] = (char *)data_value;
            }

           /* Set up the environment.
            */
            for (env_length = 0, envp = environ; *envp; envp++) {
                env_length++;
            }
            env_length++;  // add 1 for KEXT_LAUNCH_USERID
            if (ext_auth_mbox >= 0)
                env_length++;  // add 1 for KEXT_AUTHORIZATION
            env_length++;  // add 1 for terminating NULL

            kextload_env = (char **)malloc(env_length * sizeof(char *));
            if (!kextload_env) {
                kextd_error_log("can't build environment for kextload");
                exit(-1);
            }

            bzero(kextload_env, env_length * sizeof(char *));

            for (envp = environ, envp_copy = kextload_env;
                 *envp; envp++, envp_copy++) {

                *envp_copy = *envp;
            }

            sprintf(kextd_launch, "KEXTD_LAUNCH_USERID=%d", logged_in_uid);
            *envp_copy++ = kextd_launch;
            if (kextd_ext_auth_env)
                *envp_copy++ = kextd_ext_auth_env;
            *envp_copy++ = NULL;

            if (execve(k_argv[0], k_argv, kextload_env) == -1) {
                kextd_error_log("can't exec kextload (%s)", strerror(errno));
                exit(-1);  // exit the child immediately
            }
            if (k_argv) {
                free(k_argv);
                k_argv = NULL;
            }
            if (kextload_env) {
                free(kextload_env);
                kextload_env = NULL;
            }

        } else {
            // parent; schedule a call to waitpid() later through the run loop
            scheduleAnother = true;
            goto finish;
        }
    }

finish:

    if (ext_auth_mbox)
        fclose(ext_auth_mbox);
    if (kextd_ext_auth_env) {
        free(kextd_ext_auth_env);
        kextd_ext_auth_env = NULL;
    }

    if (loadData)     CFRelease(loadData);
    if (loadDataDict) CFRelease(loadDataDict);
    if (errorString)  CFRelease(errorString);

    if (scheduleAnother) {
        PTLockTakeLock(gRunLoopSourceLock);
        CFRunLoopSourceSignal(gNonsecureKextRunLoopSource);
        CFRunLoopWakeUp(gMainRunLoop);
        PTLockUnlock(gRunLoopSourceLock);
    }

    return;
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
    CFDataRef loadData = NULL;  // must release

    loadData = CFDataCreate(kCFAllocatorDefault, load_data, load_data_length);
    if (!loadData) {
        result = KERN_FAILURE;
        goto finish;
    }

    CFArrayAppendValue(gPendedKextloadOperations, loadData);

#endif /* NO_CFUserNotification */


finish:

#ifndef NO_CFUserNotification
    if (loadData) CFRelease(loadData);
#endif /* NO_CFUserNotification */
    gClientUID = -1;
    return result;
}

#ifndef NO_CFUserNotification

/*******************************************************************************
*
*******************************************************************************/
static void _kextd_raise_security_alert(KXKextRef aKext)
{
    CFMutableDictionaryRef alertDict = NULL;  // must release
    CFMutableArrayRef alertMessageArray = NULL; // must release
    CFURLRef iokitFrameworkBundleURL = NULL;  // must release
    SInt32 userNotificationError = 0;

    gResendSecurityAlertKextPersonalities = false;

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
        CFSTR("The file \""));
    CFArrayAppendValue(alertMessageArray, KXKextGetBundleDirectoryName(aKext));
    CFArrayAppendValue(alertMessageArray,
        CFSTR("\" has problems that may reduce the security of "
            "your computer. You should contact the manufacturer of the "
            "product you are using for a new version. If you are sure the "
            "file is OK, you can allow the application to use it, or fix "
            "it and then use it. If you click Don't Use, any other files "
            "that depend on this file will not be used."));

    CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
        iokitFrameworkBundleURL);
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
        CFSTR("The program you are using needs to use a system file that may "
              "reduce the security of your computer."));
    CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
        CFSTR("Don't Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlternateButtonTitleKey,
        CFSTR("Fix and Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationOtherButtonTitleKey,
        CFSTR("Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
        alertMessageArray);

    gSecurityAlertResponse = 0;

    gSecurityNotification = CFUserNotificationCreate(kCFAllocatorDefault,
        0 /* time interval */, kCFUserNotificationCautionAlertLevel,
        &userNotificationError, alertDict);
    if (!gSecurityNotification) {
        kextd_error_log(
            "error creating user notification (%d)", userNotificationError);
        goto finish;
    }

    gSecurityAlertKext = aKext;

    PTLockTakeLock(gRunLoopSourceLock);
    CFRunLoopSourceSignal(gNonsecureKextRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);
    PTLockUnlock(gRunLoopSourceLock);

finish:

    if (alertDict)               CFRelease(alertDict);
    if (alertMessageArray)       CFRelease(alertMessageArray);
    if (iokitFrameworkBundleURL) CFRelease(iokitFrameworkBundleURL);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void _kextd_raise_failure_alert(KXKextRef aKext)
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
        CFSTR("The file \""));
    CFArrayAppendValue(alertMessageArray, KXKextGetBundleDirectoryName(aKext));
    CFArrayAppendValue(alertMessageArray,
        CFSTR("\" could not be fixed, but it will be used anyway."));
    CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
        iokitFrameworkBundleURL);
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
        CFSTR("File Access Error"));
    CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
        CFSTR("OK"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
        alertMessageArray);

    gFailureAlertResponse = 0;

    gFailureNotification = CFUserNotificationCreate(kCFAllocatorDefault,
        0 /* time interval */, kCFUserNotificationCautionAlertLevel,
        &userNotificationError, alertDict);
    if (!gFailureNotification) {
        kextd_error_log(
            "error creating user notification (%d)", userNotificationError);
        goto finish;
    }

    PTLockTakeLock(gRunLoopSourceLock);
    CFRunLoopSourceSignal(gNonsecureKextRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);
    PTLockUnlock(gRunLoopSourceLock);

finish:

    if (alertDict)               CFRelease(alertDict);
    if (alertMessageArray)       CFRelease(alertMessageArray);
    if (iokitFrameworkBundleURL) CFRelease(iokitFrameworkBundleURL);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void _kextd_poll_alert_response(void)
{
    int userNotificationResult = 0;
    Boolean authorizationReady = false;

    PTLockTakeLock(gUserAuthorizationLock);
    authorizationReady = gAuthorizationReady;
    PTLockUnlock(gUserAuthorizationLock);

    if (authorizationReady && gSecurityAlertKext) {
        PTLockTakeLock(gUserAuthorizationLock);
        gWaitingForAuthorization = false;
        gAuthorizationReady = false;
        PTLockUnlock(gUserAuthorizationLock);

        if (auth_result != errAuthorizationSuccess) {
            gResendSecurityAlertKextPersonalities = false;
            gSecurityAlertKext = NULL;
            goto finish;
        }

       KXKextManagerRequalifyKext(gKextManager, gSecurityAlertKext);

       if (gSecurityAlertResponse == kCFUserNotificationAlternateResponse) {
            KXKextManagerError makeSecureResult = _KXKextMakeSecure(gSecurityAlertKext);

            if (makeSecureResult == kKXKextManagerErrorNone) {
                gResendSecurityAlertKextPersonalities = true;
            } else if (makeSecureResult == kKXKextManagerErrorFileAccess) {
                KXKextMarkAuthentic(gSecurityAlertKext);
                _kextd_raise_failure_alert(gSecurityAlertKext);
            } else {
                gSecurityAlertKext = NULL;
            }
            goto finish;
        } else if (gSecurityAlertResponse == kCFUserNotificationOtherResponse) {
            KXKextMarkAuthentic(gSecurityAlertKext);
            gResendSecurityAlertKextPersonalities = true;
            goto finish;
        }

    } else if (gFailureNotification && gSecurityAlertKext) {

        userNotificationResult = CFUserNotificationReceiveResponse(
            gFailureNotification, 1, &gFailureAlertResponse);

        if (userNotificationResult != 0) {
            // still waiting....
            goto finish;
        } else {
            gResendSecurityAlertKextPersonalities = true;
            CFRelease(gFailureNotification);
            gFailureNotification = NULL;
        }

    } else if (gSecurityNotification && gSecurityAlertKext) {

        userNotificationResult = CFUserNotificationReceiveResponse(
            gSecurityNotification, 1, &gSecurityAlertResponse);

        if (userNotificationResult != 0) {
            // still waiting....
            goto finish;
        } else {
            CFRelease(gSecurityNotification);
            gSecurityNotification = NULL;
        }

        if (gSecurityAlertResponse == kCFUserNotificationDefaultResponse) {
            // The default response is to not load the kext.
            gResendSecurityAlertKextPersonalities = false;
            gSecurityAlertKext = NULL;
            goto finish;
        } else if (gSecurityAlertResponse == kCFUserNotificationAlternateResponse ||
                   gSecurityAlertResponse == kCFUserNotificationOtherResponse) {

            PTLockTakeLock(gUserAuthorizationLock);
            gWaitingForAuthorization = true;
            gAuthorizationReady = false;
            PTLockUnlock(gUserAuthorizationLock);

            _kextd_launch_authorization();
        }
    }

finish:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _kextd_launch_authorization(void)
{
    pthread_attr_t auth_thread_attr;
    pthread_t      auth_request_thread;

    pthread_attr_init(&auth_thread_attr);
    pthread_create(&auth_request_thread,
        &auth_thread_attr,
        _kextd_authorize_user, NULL);
    pthread_detach(auth_request_thread);
}

/*******************************************************************************
*
*******************************************************************************/
void * _kextd_authorize_user(void * arg)
{
    uid_t real_euid = geteuid();
    AuthorizationItem fixkextright = { "system.kext.make_secure", 0,
        NULL, 0 };
    const AuthorizationItemSet fixkextrightset = { 1, &fixkextright };
    int flags = kAuthorizationFlagExtendRights |
        kAuthorizationFlagInteractionAllowed;

    if (seteuid(logged_in_uid) != 0) {
        kextd_error_log("call to seteuid() failed");
        auth_result = errAuthorizationDenied;
        PTLockTakeLock(gUserAuthorizationLock);
        gWaitingForAuthorization = false;
        gAuthorizationReady = true;
        PTLockUnlock(gUserAuthorizationLock);
        goto finish;  // okay to do this, no lock is held
    }

    auth_result = AuthorizationCopyRights(gAuthRef, &fixkextrightset,
        NULL, flags, NULL);

    seteuid(real_euid);

    PTLockTakeLock(gUserAuthorizationLock);
    gWaitingForAuthorization = false;
    gAuthorizationReady = true;
    PTLockUnlock(gUserAuthorizationLock);

finish:

    pthread_exit(NULL);
    return NULL;
}

#endif /* NO_CFUserNotification */
