/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#if !TARGET_OS_EMBEDDED

#include "IOPlatformSupportPrivate.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <notify.h>

// find what files to include
#if 0
#define DLOG(format)                    syslog(LOG_DEBUG, "IONTSLib: " format)
#define DLOG1(format, args ...)         syslog(LOG_DEBUG, "IONTSLib: " format, args)
#define DLOG_ERROR(format)              syslog(LOG_ERR, "IONTSLib: " format)
#define DLOG_ERROR1(format, args ...)   syslog(LOG_ERR, "IONTSLib: " format, args)
#else
#define DLOG(format)
#define DLOG1(format, args ...)
#define DLOG_ERROR(format)
#define DLOG_ERROR1(format, args ...)
#endif


/* CopyModel
 *
 * Argument:
 *	char** model: returns the model name
 *  uint32_t *majorRev: returns the major revision number
 *  uint32_t *minorRev: returns the minor revision number
 * Return value:
 * 	true on success
 * Note: In case of success, CopyModel allocates memory for *model,
 *       it is the caller's responsibility to free *model.
 * Note: model format is expected to be %s%u,%u
 *       If the revision number is partial, the value is assumed to be %u,0 or 0,0
 */

static Boolean CopyModel(char** model, uint32_t *majorRev, uint32_t *minorRev)
{
    int mib[2];
    char *machineModel; // must free
    char *revStr;
    int count;
    unsigned long modelLen;
    Boolean success = true;
    size_t length = 1024;
    
    if (!model || !majorRev || !minorRev) {
        DLOG_ERROR("CopyModel: Passing NULL arguments\n");
        return false;
    }

    machineModel = malloc(length);
    mib[0] = CTL_HW;
    mib[1] = HW_MODEL;
    if (sysctl(mib, 2, machineModel, &length, NULL, 0)) {
        printf("CopyModel: sysctl (error %d)\n", errno);
        success = false;
        goto exit;
    }
    
    modelLen = strcspn(machineModel, "0123456789");
    if (modelLen == 0) {
        DLOG_ERROR("CopyModel: Could not find machine model name\n");
        success = false;
        goto exit;
    }
    *model = strndup(machineModel, modelLen);
    if (*model == NULL) {
        DLOG_ERROR("CopyModel: Could not find machine model name\n");
        success = false;
        goto exit;
    }
    
    *majorRev = 0;
    *minorRev = 0;
    revStr = strpbrk(machineModel, "0123456789");
    if (!revStr) {
        DLOG_ERROR("CopyModel: Could not find machine version number, inferred value is 0,0\n");
        success = true;
        goto exit;
    }
    
    count = sscanf(revStr, "%d,%d", majorRev, minorRev);
    if (count < 2) {
        DLOG_ERROR("CopyModel: Could not find machine version number\n");
        if (count<1) {
            *majorRev = 0;
        }
        *minorRev = 0;
        success = true;
        goto exit;
    }

exit:
    if (machineModel) free(machineModel);
    if (!success) {
        if (*model) free(*model);
        *model = NULL;
        *majorRev = 0;
        *minorRev = 0;
    }
    return success;
} // CopyModel

/* IOSMCKeyProxyPresent
 *
 * Return value:
 * 	true if system has SMC Key Proxy
 */
Boolean IOSMCKeyProxyPresent()
{
    char* model = NULL; // must free
    uint32_t majorRev;
    uint32_t minorRev;
    Boolean success = false;
    
    Boolean modelFound = CopyModel(&model, &majorRev, &minorRev);
    if (!modelFound) {
        DLOG_ERROR("IOSMCKeyProxySupported: Could not find machine model\n");
        return false;
    }
    if (!strncmp(model, "MacBookPro", 10) && (majorRev <=7))
        goto exit;
    if (!strncmp(model, "MacBookAir", 10) && (majorRev <=2))
        goto exit;
    if (!strncmp(model, "MacBook", 10))
        goto exit;
    if (!strncmp(model, "iMac", 10)       && (majorRev <=11))
        goto exit;
    if (!strncmp(model, "Macmini", 10)    && (majorRev <=3))
        goto exit;
    if (!strncmp(model, "MacPro", 10)     && (majorRev <=5))
        goto exit;
    if (!strncmp(model, "Xserve", 10))
        goto exit;
    
    success = true;
    DLOG("Machine appears to have SMC Key Proxy\n");
    
exit:
    if (model) free(model);
    return success;
} // IOSMCKeyProxyPresent

/* IONoteToSelfSupported
 *
 * Return value:
 * 	true if system supports Note To Self
 */
Boolean IONoteToSelfSupported()
{
    char* model = NULL; // must free
    uint32_t majorRev;
    uint32_t minorRev;
    Boolean success = false;
    
    Boolean modelFound = CopyModel(&model, &majorRev, &minorRev);
    if (!modelFound) {
        DLOG_ERROR("IONoteToSelfSupported: Could not find machine model\n");
        return false;
    }
    if (!strncmp(model, "MacBookPro", 10) && (majorRev < 5 ||
                                              (majorRev == 5 && minorRev <= 2)))
        goto exit;
    if (!strncmp(model, "MacBookAir", 10) && (majorRev <=2))
        goto exit;
    if (!strncmp(model, "MacBook", 10)    && (majorRev <=5))
        goto exit;
    if (!strncmp(model, "iMac", 10)       && (majorRev <=9))
        goto exit;
    if (!strncmp(model, "Macmini", 10)    && (majorRev <=3))
        goto exit;
    if (!strncmp(model, "MacPro", 10))
        goto exit;
    if (!strncmp(model, "Xserve", 10))
        goto exit;
    
    success = true;
    DLOG("Machine appears to be capable of Note To Self\n");
    
exit:
    if (model) free(model);
    return success;
} // IONoteToSelfSupported

/* IOAuthenticatedRestartSupported
 *
 * Return value:
 * 	true if system supports Authenticated Restart, using Note To Self or SMC Key Proxy
 */
Boolean IOAuthenticatedRestartSupported()
{
    return IONoteToSelfSupported() || IOSMCKeyProxyPresent();
} // IOAuthenticatedRestartSupported

static CFTypeRef _copyRootDomainProperty(CFStringRef key)
{
    io_registry_entry_t     platformReg = IO_OBJECT_NULL;
    CFTypeRef               obj = NULL;

    platformReg = IORegistryEntryFromPath( kIOMasterPortDefault,
                                          kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if (IO_OBJECT_NULL != platformReg)
    {
        obj = IORegistryEntryCreateCFProperty(
                               platformReg,
                               key,
                               kCFAllocatorDefault,
                               kNilOptions);
    }
    IOObjectRelease(platformReg);
    
    return obj;
}

/*
 *
 * Defaults
 *
 */
IOReturn IOPlatformCopyFeatureDefault(
    CFStringRef   platformSettingKey,
    CFTypeRef     *outValue)
{
    /* IOPPF should publish the platform defaults dictionary under IOPMrootDomain
     * under the key 'IOPlatformFeatureDefaults'
     */
    CFDictionaryRef         platformFeatures = NULL;
    CFTypeRef               returnObj = NULL;
    
    if (!platformSettingKey || !outValue) {
        return kIOReturnBadArgument;
    }

    platformFeatures = _copyRootDomainProperty(CFSTR("IOPlatformFeatureDefaults"));
    
    if (platformFeatures) {
        returnObj = CFDictionaryGetValue(platformFeatures, platformSettingKey);
        if (returnObj) {
            *outValue = CFRetain(returnObj);
        }
        CFRelease(platformFeatures);
    }
    
    if (*outValue) {
        return kIOReturnSuccess;
    } else {
        return kIOReturnUnsupported;
    }
}

/*
 *
 * Active
 *
 */
IOReturn IOPlatformCopyFeatureActive(
    CFStringRef   platformSettingKey,
    CFTypeRef     *outValue)
{
    
    if (!platformSettingKey || !outValue) {
        return kIOReturnBadArgument;
    }
    
    if (CFEqual(platformSettingKey, kIOPlatformTCPKeepAliveDuringSleep))
    {
        int keepAliveIsValid = 0;
        keepAliveIsValid = IOPMGetValueInt(kIOPMTCPKeepAliveIsActive);
        
        if (keepAliveIsValid) {
            *outValue = kCFBooleanTrue;
        } else {
            *outValue = kCFBooleanFalse;
        }
        return kIOReturnSuccess;
    } else
    {
        return IOPlatformCopyFeatureDefault(platformSettingKey, outValue);
    }
}

#endif
