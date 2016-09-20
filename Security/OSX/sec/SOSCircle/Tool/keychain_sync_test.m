//
//  keychain_sync_test.c
//  sec
//
//  Created by Mitch Adler on 7/8/16.
//
//

#include "keychain_sync_test.h"

#include "secToolFileIO.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#import <Foundation/Foundation.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>

#import "NSFileHandle+Formatting.h"

int
keychain_sync_test(int argc, char * const *argv)
{
    NSFileHandle *fhout = [NSFileHandle fileHandleWithStandardOutput];
    NSFileHandle *fherr = [NSFileHandle fileHandleWithStandardError];
    /*
     "Keychain Syncing test"

     */
    int result = 0;
    NSError* error = nil;
    CFErrorRef cfError = NULL;

    static int verbose_flag = 0;
    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"verbose",     no_argument,        &verbose_flag, 1},
        {"brief",       no_argument,        &verbose_flag, 0},
        /* These options don’t set a flag.
         We distinguish them by their indices. */
        {"enabled-peer-views",      required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    static const char * params = "abc:d:f:";

    /* getopt_long stores the option index here. */
    int option_index = 0;

    NSArray<NSString*>* viewList = nil;

    int opt_result = 0;
    while (opt_result != -1) {
        opt_result = getopt_long (argc, argv, params, long_options, &option_index);
        switch (opt_result) {
            case 'p': {
                NSString* parameter = [NSString stringWithCString: optarg encoding:NSUTF8StringEncoding];

                viewList = [parameter componentsSeparatedByString:@","];
                
                }
                break;
            case -1:
                break;
            default:
                return 2;
        }

    }

    if (viewList) {
        CFBooleanRef result = SOSCCPeersHaveViewsEnabled((__bridge CFArrayRef) viewList, &cfError);
        if (result != NULL) {
            [fhout writeFormat: @"Views: %@\n", viewList];
            [fhout writeFormat: @"Enabled on other peers: %@\n", CFBooleanGetValue(result) ? @"yes" : @"no"];
        }
    }

    if (cfError != NULL) {
        [fherr writeFormat: @"Error: %@\n", cfError];
    }

    if (error != NULL) {
        [fherr writeFormat: @"Error: %@\n", error];
    }

    return result;
}
