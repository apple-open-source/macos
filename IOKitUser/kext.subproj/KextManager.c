#include "KXKextManager.h"
#include "KextManager.h"
#include "kextmanager_mig.h"
#include <mach/kmod.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

static const char * KEXTD_SERVER_NAME = "com.apple.KernelExtensionServer";

CFURLRef KextManagerCreateURLForBundleIdentifier(
    CFAllocatorRef allocator,
    CFStringRef    bundleIdentifier)
{
    CFURLRef bundleURL = NULL;  // returned

    kern_return_t kern_result = KERN_FAILURE;
    char bundle_id[KMOD_MAX_NAME] = "";

    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;

    char bundle_path[MAXPATHLEN] = "";
    CFStringRef bundlePath = NULL;  // must free
    KXKextManagerError kext_result = kKXKextManagerErrorNone;

    if (!bundleIdentifier) {
        goto finish;
    }

    if (!CFStringGetCString(bundleIdentifier,
        bundle_id, sizeof(bundle_id) - 1, kCFStringEncodingMacRoman)) {
        goto finish;
    }

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_path_for_bundle_id(kextd_port, bundle_id,
        bundle_path, &kext_result);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    if (kext_result != kKXKextManagerErrorNone) {
        goto finish;
    }

    bundlePath = CFStringCreateWithCString(kCFAllocatorDefault,
        bundle_path, kCFStringEncodingMacRoman);
    if (!bundlePath) {
        goto finish;
    }

    bundleURL = CFURLCreateWithFileSystemPath(allocator,
        bundlePath, kCFURLPOSIXPathStyle, true);

finish:

    if (bundlePath)  CFRelease(bundlePath);

    return bundleURL;
}

CFArrayRef _KextManagerCreatePropertyValueArray(
    CFAllocatorRef allocator,
    CFStringRef    propertyKey)
{
    CFMutableArrayRef valueArray = NULL;  // returned
    CFDataRef         xmlData = NULL;  // must release

    kern_return_t kern_result = KERN_FAILURE;
    char property_key[128] = "";  // matches prop_key_t in .defs file

    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;

    char * xml_data = NULL;  // must vm_deallocate()
    int    xml_data_length = 0;

    CFStringRef errorString = NULL;  // must release

    if (!propertyKey || (CFStringGetLength(propertyKey) > 127)) {
        goto finish;
    }

    if (!CFStringGetCString(propertyKey,
        property_key, sizeof(property_key) - 1, kCFStringEncodingMacRoman)) {
        goto finish;
    }

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_create_property_value_array(kextd_port,
        property_key, &xml_data, &xml_data_length);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    xmlData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xml_data,
        xml_data_length, kCFAllocatorNull);
    if (!xmlData) {
        goto finish;
    }

    valueArray = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
        xmlData, kCFPropertyListImmutable, &errorString);
    if (!valueArray) {
        goto finish;
    }

    if (CFGetTypeID(valueArray) != CFArrayGetTypeID()) {
        CFRelease(valueArray);
        valueArray = NULL;
        goto finish;
    }

finish:

    if (errorString)  CFRelease(errorString);
    if (xmlData)      CFRelease(xmlData);

    if (xml_data) {
        kern_result = vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
            xml_data_length);
        if (kern_result != KERN_SUCCESS) {
            // FIXME: Log a message here?
        }
    }
    return valueArray;
}


void _KextManagerUserDidLogIn(uid_t euid, AuthorizationExternalForm authref)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_user_did_log_in(kextd_port, euid,
        authref);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

finish:
    return;
}

void _KextManagerUserWillLogOut(uid_t euid)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_user_will_log_out(kextd_port, euid);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

finish:
    return;
}


uid_t _KextManagerGetLoggedInUserid()
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;
    uid_t euid = -1;

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_get_logged_in_userid(kextd_port, &euid);
    if (kern_result != KERN_SUCCESS) {
        euid = -1;
        goto finish;
    }

finish:
    return euid;
}


void _KextManagerRecordNonsecureKextload(const char * load_data,
    size_t data_length)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   bootstrap_port = PORT_NULL;
    mach_port_t   kextd_port = PORT_NULL;

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS :
        /* service currently registered, "a good thing" (tm) */
        break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s\n",
            mach_error_string(kern_result));
        goto finish;
      default:
        /* service not currently registered, try again later */
        fprintf(stderr, "bootstrap_look_up(): %s [%d]\n",
            mach_error_string(kern_result), kern_result);
        goto finish;
    }

    kern_result = kextmanager_record_nonsecure_kextload(kextd_port,
        load_data, data_length);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

finish:
    return;
}
