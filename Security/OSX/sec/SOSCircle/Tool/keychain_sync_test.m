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

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>

#import <Foundation/Foundation.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>

#import "NSFileHandle+Formatting.h"

static char boolToChars(bool val, char truechar, char falsechar) {
    return val? truechar: falsechar;
}

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
    __block CFErrorRef cfError = NULL;

    static int verbose_flag = 0;
    bool dump_pending = false;

    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"verbose",     no_argument,        &verbose_flag, 1},
        {"brief",       no_argument,        &verbose_flag, 0},
        /* These options don’t set a flag.
         We distinguish them by their indices. */
        {"enabled-peer-views",      required_argument, 0, 'p'},
        {"message-pending-state",   no_argument,       0, 'm'},
        {0, 0, 0, 0}
    };
    static const char * params = "p:m";

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
            case 'm':
                dump_pending = true;
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

    if (dump_pending) {
        CFArrayRef peers = SOSCCCopyPeerPeerInfo(&cfError);
        [fhout writeFormat: @"Dumping state for %ld peers\n", CFArrayGetCount(peers)];

        CFArrayForEach(peers, ^(const void *value) {
            SOSPeerInfoRef thisPeer = (SOSPeerInfoRef) value;
            if (thisPeer) {
                CFReleaseNull(cfError);
                bool message = SOSCCMessageFromPeerIsPending(thisPeer, &cfError);
                if (!message && cfError != NULL) {
                    [fherr writeFormat: @"Error from SOSCCMessageFromPeerIsPending: %@\n", cfError];
                }
                CFReleaseNull(cfError);
                bool send = SOSCCSendToPeerIsPending(thisPeer, &cfError);
                if (!message && cfError != NULL) {
                    [fherr writeFormat: @"Error from SOSCCSendToPeerIsPending: %@\n", cfError];
                }
                CFReleaseNull(cfError);

                [fhout writeFormat: @"Peer: %c%c %@\n", boolToChars(message, 'M', 'm'), boolToChars(send, 'S', 's'), thisPeer];
            } else {
                [fherr writeFormat: @"Non SOSPeerInfoRef in array: %@\n", value];
            }
        });
    }

    if (cfError != NULL) {
        [fherr writeFormat: @"Error: %@\n", cfError];
    }

    if (error != NULL) {
        [fherr writeFormat: @"Error: %@\n", error];
    }

    return result;
}
