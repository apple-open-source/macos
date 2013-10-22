//
//  SecdKeychainUtilities.c
//  sec
//
//  Created by Mitch Adler on 6/11/13.
//
//

#include "SecdTestKeychainUtilities.h"

#include <test/testmore.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

//#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void kc_dbhandle_reset(void);

void secd_test_setup_temp_keychain(const char* test_prefix, dispatch_block_t do_before_reset)
{
    CFStringRef tmp_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("/tmp/%s.%X/"), test_prefix, arc4random());
    CFStringRef keychain_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@Library/Keychains"), tmp_dir);
    
    CFStringPerformWithCString(keychain_dir, ^(const char *keychain_dir_string) {
        ok_unix(mkpath_np(keychain_dir_string, 0755), "Create temp dir");
        
        printf("Created temporary directory %s\n", keychain_dir_string);
    });
    
    
    /* set custom keychain dir, reset db */
    CFStringPerformWithCString(tmp_dir, ^(const char *tmp_dir_string) {
        SetCustomHomeURL(tmp_dir_string);
    });

    if(do_before_reset)
        do_before_reset();
    
    kc_dbhandle_reset();
}
