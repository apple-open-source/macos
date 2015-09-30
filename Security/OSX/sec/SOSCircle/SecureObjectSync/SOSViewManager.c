/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*
 * SOSViewManager.c -  Implementation of a view manager
 */

#include <Security/SecureObjectSync/SOSViewManager.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#if 0
/* SOSViewManager implementation. */
struct __OpaqueSOSViewManager {
    CFRuntimeBase _base;
    CFMutableDictionaryRef views;
};

const CFStringRef kSOSContextChildInfoKey = CFSTR("cntx");
const CFStringRef kSOSFunctionChildInfoKey = CFSTR("fctn");
const CFStringRef kSOSViewNamesChildInfoKey = CFSTR("vwns");


static CFStringRef SOSViewManagerCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSViewManagerRef vmgr = (SOSViewManagerRef)cf;
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions, CFSTR("<ViewManager %@ >"), vmgr->views);
    return desc;
}

static void SOSViewManagerDestroy(CFTypeRef cf) {
    SOSViewManagerRef vmgr = (SOSViewManagerRef)cf;
    CFReleaseSafe(vmgr->views);
}

CFGiblisFor(SOSViewManager);


static SOSViewManagerRef SOSViewManagerCreate(CFAllocatorRef allocator, CFErrorRef *error) {
    SOSViewManagerRef vmgr = NULL;
    vmgr = CFTypeAllocate(SOSViewManager, struct __OpaqueSOSViewManager, allocator);
    if (vmgr)
        vmgr->views = CFDictionaryCreateMutableForCFTypes(allocator);
    return vmgr;
}

CFGiblisGetSingleton(SOSViewManagerRef, SOSGetViewManager, sSOSViewManager,  ^{
    *sSOSViewManager = SOSViewManagerCreate(kCFAllocatorDefault, NULL);
});


static CFStringRef CFStringCreateWithViewNames(CFArrayRef viewNames) {
    CFIndex count = CFArrayGetCount(viewNames);
    CFMutableArrayRef mvn = CFArrayCreateMutableCopy(kCFAllocatorDefault, count, viewNames);
    CFArraySortValues(mvn, CFRangeMake(0, count), (CFComparatorFunction)CFStringCompare, 0);
    CFStringRef string = CFStringCreateByCombiningStrings(kCFAllocatorDefault, mvn, CFSTR(":"));
    CFRelease(mvn);
    return string;
}

static SOSViewRef SOSViewManangerCopyViewWithName(SOSViewManagerRef vmgr, CFMutableDictionaryRef referencedViews, CFStringRef viewName, bool isConcrete, CFErrorRef *error) {
    SOSViewRef view = (SOSViewRef)CFDictionaryGetValue(vmgr->views, viewName);
    if (view) {
        if (isConcrete)
            SOSViewSetConcrete(view, true);
        CFRetain(view);
    } else {
        view = SOSViewCreate(CFGetAllocator(vmgr), isConcrete, NULL, error);
        if (view) {
            CFDictionarySetValue(vmgr->views, viewName, view);
        }
        // TODO: Query for the initial manifest.
    }
    if (view) {
        if (isConcrete)
            CFDictionarySetValue(referencedViews, viewName, kCFBooleanTrue);
        else if (!CFDictionaryContainsKey(referencedViews, viewName))
            CFDictionarySetValue(referencedViews, viewName, kCFBooleanFalse);
    }
    return view;
}

static SOSViewRef SOSViewManangerCopyCompositeViewWithNames(SOSViewManagerRef vmgr, CFMutableDictionaryRef referencedViews, CFArrayRef viewNames, CFErrorRef *error) {
    CFStringRef compositeName = CFStringCreateWithViewNames(viewNames);
    SOSViewRef compositeView = (SOSViewRef)CFDictionaryGetValue(vmgr->views, compositeName);
    if (compositeView) {
        CFDictionarySetValue(referencedViews, compositeName, kCFBooleanTrue);
        CFRetain(compositeView);
    } else {
        compositeView = SOSViewCreate(CFGetAllocator(vmgr), true, NULL, error);
        if (compositeView) {
            // Find the views for each name, and add the new view as a child to each one.
            CFStringRef viewName;
            CFArrayForEachC(viewNames, viewName) {
                SOSViewRef parent = SOSViewManangerCopyViewWithName(vmgr, referencedViews, viewName, false, error);
                if (!parent) {
                    CFReleaseNull(compositeView);
                    break;
                }
                SOSViewAddChild(parent, compositeView);
                // Update the composite view's manifest by adding each parents manifest.
                // TODO: Potentially move this out of the loop and create a single multi way manifest union operation
                SOSManifestRef pmf = SOSViewCopyManifest(parent, error);
                SOSViewUpdateManifest(compositeView, kSOSDataSourceSOSTransaction, NULL, pmf, error);
                CFReleaseSafe(pmf);
            }
            CFDictionarySetValue(vmgr->views, viewName, compositeView);
        }
    }
    CFReleaseSafe(compositeName);
    return compositeView;
}

static bool SOSViewManangerAddChildWithInfo(SOSViewManagerRef vmgr, CFMutableDictionaryRef referencedViews, CFDictionaryRef childInfo, CFErrorRef *error) {
    CFArrayRef viewNames = (CFArrayRef)CFDictionaryGetValue(childInfo, kSOSViewNamesChildInfoKey);
//    const void *context = (const void *)CFDictionaryGetValue(childInfo, kSOSContextChildInfoKey);
//    const void *func = (const void *)CFDictionaryGetValue(childInfo, kSOSFunctionChildInfoKey);

    CFIndex count = CFArrayGetCount(viewNames);
    if (count == 1) {
        CFStringRef key = CFArrayGetValueAtIndex(viewNames, 0);
        SOSViewRef view = SOSViewManangerCopyViewWithName(vmgr, referencedViews, key, true, error);
        // TODO: Fix this...
        //SOSViewAddClient(view, context, func);
        if (view) count++;  // TODO: REMOVE MOVE -- HERE ONLY FOR COMPILER WARNING
    } else if (count > 1) {
        SOSViewRef view = SOSViewManangerCopyCompositeViewWithNames(vmgr, referencedViews, viewNames, error);
        // TODO: Fix this...
        //SOSViewAddClient(view, context, func);
        if (view) count++;  // TODO: REMOVE MOVE -- HERE ONLY FOR COMPILER WARNING
    }

    return true;
}

struct SOSViewManagerContext {
    SOSViewManagerRef vmgr;
    CFDictionaryRef referencedViews;
};

static void SOSViewManagerUpdateView(const void *key, const void *value, void *context) {
    struct SOSViewManagerContext *vmc = context;
    CFBooleanRef isConcrete = CFDictionaryGetValue(vmc->referencedViews, key);
    if (!isConcrete) {
        CFDictionaryRemoveValue(vmc->vmgr->views, key);
    } else if (CFBooleanGetValue(isConcrete) == 0) {
        SOSViewRef view = (SOSViewRef)CFDictionaryGetValue(vmc->vmgr->views, key);
        SOSViewSetConcrete(view, false);
    }
}

bool SOSViewManagerSetChildren(SOSViewManagerRef vmgr, CFArrayRef children, CFErrorRef *error) {
    bool ok = true;
    CFMutableDictionaryRef referencedViews = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef childInfo;
    CFArrayForEachC(children, childInfo) {
        ok &= SOSViewManangerAddChildWithInfo(vmgr, referencedViews, childInfo, error);
    }

    // Potentially populate all views here.

    // Cleanup, remove any views we no longer reference, and set any views which need not be concrete as such.
    struct SOSViewManagerContext vmc = {
        .vmgr = vmgr,
        .referencedViews = referencedViews,
    };
    CFDictionaryApplyFunction(vmgr->views, SOSViewManagerUpdateView, &vmc);
    CFRetainSafe(referencedViews);

    return ok;
}
#endif
