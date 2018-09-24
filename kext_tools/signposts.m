//
//  signposts.m
//  kext_tools
//
//  Copyright 2018 Apple Inc. All rights reserved.
//
#import <Foundation/Foundation.h>

#import "kext_tools_util.h"
#import "signposts.h"

void
signpost_kext_properties(OSKextRef theKext, os_signpost_id_t spid)
{
    os_signpost_event_emit(get_signpost_log(), spid, "KextURL", "%@", OSKextGetURL(theKext));
    os_signpost_event_emit(get_signpost_log(), spid, "KextBundleID", "%@", OSKextGetIdentifier(theKext));
}

os_signpost_id_t
generate_signpost_id(void)
{
    return os_signpost_id_generate(get_signpost_log());
}
