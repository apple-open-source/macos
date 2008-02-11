#include <mach/mach.h>
#include <TargetConditionals.h>
#include <mach/kmod.h>
#include <sys/param.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <stdarg.h>

#include "KXKextManager.h"
#include "KextManager.h"
#include "KextManagerPriv.h"
#include "kextmanager_mig.h"

static kern_return_t get_kextd_port(mach_port_t *kextd_port); // internal convenience function

kern_return_t _KextManagerRecordPathForBundleID(CFStringRef kextBundleID,
    CFStringRef kextPath)
{
    char bundle_id[KMOD_MAX_NAME] = "";
    char kext_path[MAXPATHLEN] = "";
    mach_port_t   kextd_port = MACH_PORT_NULL;
    kern_return_t kern_result = KERN_FAILURE;

    if (!kextBundleID || !kextPath) {
        goto finish;
    }

    if (!CFStringGetCString(kextBundleID, bundle_id, sizeof(bundle_id),
        kCFStringEncodingUTF8)) {

        goto finish;
    }

    if (!CFStringGetCString(kextPath, kext_path, sizeof(kext_path),
        kCFStringEncodingUTF8)) {

        goto finish;
    }

    kern_result = get_kextd_port(&kextd_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = kextmanager_record_path_for_bundle_id(kextd_port,
        bundle_id, kext_path);

finish:
    return kern_result;
}

CFURLRef KextManagerCreateURLForBundleIdentifier(
    CFAllocatorRef allocator,
    CFStringRef    bundleIdentifier)
{
    CFURLRef bundleURL = NULL;  // returned

    kern_return_t kern_result = KERN_FAILURE;
    char bundle_id[KMOD_MAX_NAME] = "";

    mach_port_t   kextd_port = MACH_PORT_NULL;

    char bundle_path[MAXPATHLEN] = "";
    CFStringRef bundlePath = NULL;  // must free
    KXKextManagerError kext_result = kKXKextManagerErrorNone;
    kext_result_t tmpRes;

    if (!bundleIdentifier) {
        goto finish;
    }

    if (!CFStringGetCString(bundleIdentifier,
        bundle_id, sizeof(bundle_id) - 1, kCFStringEncodingUTF8)) {
        goto finish;
    }

    kern_result = get_kextd_port(&kextd_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = kextmanager_path_for_bundle_id(
        kextd_port, bundle_id, bundle_path, &tmpRes);
    kext_result = tmpRes;
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    if (kext_result != kKXKextManagerErrorNone) {
        goto finish;
    }

    bundlePath = CFStringCreateWithCString(kCFAllocatorDefault,
        bundle_path, kCFStringEncodingUTF8);
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
    CFAllocatorRef allocator __unused,
    CFStringRef    propertyKey)
{
    CFMutableArrayRef valueArray = NULL;  // returned
    CFDataRef         xmlData = NULL;  // must release
    CFTypeRef	      cfObj;

    kern_return_t kern_result = KERN_FAILURE;
    property_key_t property_key = "";  // matches prop_key_t in .defs file

    mach_port_t   kextd_port = MACH_PORT_NULL;

    char * xml_data = NULL;  // must vm_deallocate()
    natural_t xml_data_length = 0;

    CFStringRef errorString = NULL;  // must release

    if (!propertyKey || PROPERTYKEY_LEN < 
	    (CFStringGetMaximumSizeForEncoding(CFStringGetLength(propertyKey),
	    kCFStringEncodingUTF8))) {
        goto finish;
    }

    if (!CFStringGetCString(propertyKey,
        property_key, sizeof(property_key) - 1, kCFStringEncodingUTF8)) {
        goto finish;
    }

    kern_result = get_kextd_port (&kextd_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = kextmanager_create_property_value_array(kextd_port,
        property_key, &xml_data, &xml_data_length);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    xmlData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) xml_data,
        xml_data_length, kCFAllocatorNull);
    if (!xmlData) {
        goto finish;
    }

    cfObj = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
        xmlData, kCFPropertyListImmutable, &errorString);
    if (!cfObj) {
        goto finish;
    }

    if (CFGetTypeID(cfObj) != CFArrayGetTypeID()) {
        CFRelease(cfObj);
        goto finish;
    }
    valueArray = (CFMutableArrayRef) cfObj;

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


#if !TARGET_OS_EMBEDDED 
void _KextManagerUserDidLogIn(uid_t euid, AuthorizationExternalForm authref)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   kextd_port = MACH_PORT_NULL;

    kern_result = get_kextd_port (&kextd_port);
    if (kern_result != KERN_SUCCESS) {
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
#endif /* !TARGET_OS_EMBEDDED */

void _KextManagerUserWillLogOut(uid_t euid)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   kextd_port = MACH_PORT_NULL;

    kern_result = get_kextd_port (&kextd_port);
    if (kern_result != KERN_SUCCESS) {
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
    mach_port_t   kextd_port = MACH_PORT_NULL;
    uid_t euid = -1;

    kern_result = get_kextd_port (&kextd_port);
    if (kern_result != KERN_SUCCESS) {
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
    mach_port_t   kextd_port = MACH_PORT_NULL;

    kern_result = get_kextd_port (&kextd_port);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

    kern_result = kextmanager_record_nonsecure_kextload(kextd_port,
        (void *) load_data, data_length);
    if (kern_result != KERN_SUCCESS) {
        goto finish;
    }

finish:
    return;
}

static kern_return_t get_kextd_port(mach_port_t *kextd_port)
{
    kern_return_t kern_result = KERN_FAILURE;
    mach_port_t   bootstrap_port = MACH_PORT_NULL;
	
    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result == KERN_SUCCESS) {
        kern_result = bootstrap_look_up(bootstrap_port,
                (char *)KEXTD_SERVER_NAME, kextd_port);
    }
	
    return kern_result;
}
