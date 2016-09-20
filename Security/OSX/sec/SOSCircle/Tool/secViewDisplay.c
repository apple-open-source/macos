/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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

//  secViewDisplay.c
//  sec
//
//
//

#include "secViewDisplay.h"
#include "secToolFileIO.h"

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSViews.h>


static struct foo {
    const char *name;
    const CFStringRef *viewspec;
} string2View[] = {
    { "keychain", &kSOSViewKeychainV0 },
#undef DOVIEWMACRO
#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, DEFAULTSETTING, INITIALSYNCSETTING, ALWAYSONSETTING, BACKUPSETTING, V0SETTING) { CMDSTRING, &kSOSView##VIEWNAME, },
#include "Security/SecureObjectSync/ViewList.list"
};

static CFStringRef convertStringToView(char *viewname) {
    unsigned n;
    
    for (n = 0; n < sizeof(string2View)/sizeof(string2View[0]); n++) {
        if (strcmp(string2View[n].name, viewname) == 0)
            return *string2View[n].viewspec;
    }
    
    // Leak this, since it's a getter.
    return CFStringCreateWithCString(kCFAllocatorDefault, viewname, kCFStringEncodingUTF8);
}

static CFStringRef convertViewReturnCodeToString(SOSViewActionCode ac) {
    CFStringRef retval = NULL;
    switch(ac) {
        case kSOSCCGeneralViewError:
            retval = CFSTR("General Error"); break;
        case kSOSCCViewMember:
            retval = CFSTR("Is Member of View"); break;
        case kSOSCCViewNotMember:
            retval = CFSTR("Is Not Member of View"); break;
        case kSOSCCViewNotQualified:
            retval = CFSTR("Is not qualified for View"); break;
        case kSOSCCNoSuchView:
            retval = CFSTR("No Such View"); break;
    }
    return retval;
}

bool viewcmd(char *itemName, CFErrorRef *err) {
    char *cmd, *viewname;
    SOSViewActionCode ac = kSOSCCViewQuery;
    CFStringRef viewspec;
    
    viewname = strchr(itemName, ':');
    if(viewname == NULL) return false;
    *viewname = 0;
    viewname++;
    cmd = itemName;
    
    if(strcmp(cmd, "enable") == 0) {
        ac = kSOSCCViewEnable;
    } else if(strcmp(cmd, "disable") == 0) {
        ac = kSOSCCViewDisable;
    } else if(strcmp(cmd, "query") == 0) {
        ac = kSOSCCViewQuery;
    } else {
        return false;
    }
    
    if(strchr(viewname, ',') == NULL) { // original single value version
        viewspec = convertStringToView(viewname);
        if(!viewspec) return false;
        
        SOSViewResultCode rc = SOSCCView(viewspec, ac, err);
        CFStringRef resultString = convertViewReturnCodeToString(rc);
        
        printmsg(CFSTR("View Result: %@ : %@\n"), resultString, viewspec);
        return true;
    }
    
    if(ac == kSOSCCViewQuery) return false;
    
    // new multi-view version
    char *viewlist = strdup(viewname);
    char *token;
    char *tofree = viewlist;
    CFMutableSetRef viewSet = CFSetCreateMutable(NULL, 0, &kCFCopyStringSetCallBacks);
    
    while ((token = strsep(&viewlist, ",")) != NULL) {
        CFStringRef resultString = convertStringToView(token);
        CFSetAddValue(viewSet, resultString);
    }
    
    printmsg(CFSTR("viewSet provided is %@\n"), viewSet);
    
    free(tofree);
    
    bool retcode;
    if(ac == kSOSCCViewEnable) retcode = SOSCCViewSet(viewSet, NULL);
    else retcode = SOSCCViewSet(NULL, viewSet);
    
    fprintf(outFile, "SOSCCViewSet returned %s\n", (retcode)? "true": "false");
    
    return true;
}

bool listviewcmd(CFErrorRef *err) {
    unsigned n;
    
    for (n = 0; n < sizeof(string2View)/sizeof(string2View[0]); n++) {
        CFStringRef viewspec = *string2View[n].viewspec;
        
        SOSViewResultCode rc = SOSCCView(viewspec, kSOSCCViewQuery, err);
        CFStringRef resultString = convertViewReturnCodeToString(rc);
        
        printmsg(CFSTR("View Result: %@ : %@\n"), resultString, viewspec);
    };
    
    return true;
}
