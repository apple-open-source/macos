/*
 * Copyright (c) 2013-2016 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#include "recovery_key.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecRecoveryKey.h>

#import <CoreCDP/CoreCDP.h>


#include "secToolFileIO.h"

int
recovery_key(int argc, char * const *argv)
{
    int ch, result = 0;
    CFErrorRef error = NULL;
    BOOL hadError = false;
    SOSLogSetOutputTo(NULL, NULL); 

    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"recovery-string", no_argument, NULL, 'R' },
        {"generate",    required_argument, NULL, 'G'},
        {"set",         required_argument, NULL, 's'},
        {"get",         no_argument, NULL, 'g'},
        {"clear",       no_argument, NULL, 'c'},
        {"follow-up",   no_argument, NULL, 'F'},
        {"verifier",   no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };
    int option_index = 0;

    while ((ch = getopt_long(argc, argv, "FG:Rs:gcV:", long_options, &option_index)) != -1)
        switch  (ch) {
            case 'G': {
                NSError *nserror = NULL;
                NSString *testString = [NSString stringWithUTF8String:optarg];
                if(testString == nil)
                    return SHOW_USAGE_MESSAGE;

                SecRecoveryKey *rk = SecRKCreateRecoveryKeyWithError(testString, &nserror);
                if(rk == nil) {
                    printmsg(CFSTR("SecRKCreateRecoveryKeyWithError: %@\n"), nserror);
                    return SHOW_USAGE_MESSAGE;
                }
                NSData *publicKey = SecRKCopyBackupPublicKey(rk);
                if(publicKey == nil)
                    return SHOW_USAGE_MESSAGE;

                printmsg(CFSTR("example (not registered) public recovery key: %@\n"), publicKey);
                break;
            }
            case 'R': {
                NSString *testString = SecRKCreateRecoveryKeyString(NULL);
                if(testString == nil)
                    return SHOW_USAGE_MESSAGE;

                printmsg(CFSTR("public recovery string: %@\n"), testString);

                break;
            }
            case 's':
            {
                NSError *nserror = NULL;
                NSString *testString = [NSString stringWithUTF8String:optarg];
                if(testString == nil)
                    return SHOW_USAGE_MESSAGE;

                SecRecoveryKey *rk = SecRKCreateRecoveryKeyWithError(testString, &nserror);
                if(rk == nil) {
                    printmsg(CFSTR("SecRKCreateRecoveryKeyWithError: %@\n"), nserror);
                    return SHOW_USAGE_MESSAGE;
                }
                
                CFErrorRef cferror = NULL;
                if(!SecRKRegisterBackupPublicKey(rk, &cferror)) {
                    printmsg(CFSTR("Error from SecRKRegisterBackupPublicKey: %@\n"), cferror);
                    CFReleaseNull(cferror);
                    return SHOW_USAGE_MESSAGE;
                }
                break;
            }
            case 'g':
            {
                CFDataRef recovery_key = SOSCCCopyRecoveryPublicKey(&error);
                hadError = recovery_key == NULL;
                if(!hadError)
                    printmsg(CFSTR("recovery key: %@\n"), recovery_key);
                CFReleaseNull(recovery_key);
                break;
            }
            case 'c':
            {
                hadError = SOSCCRegisterRecoveryPublicKey(NULL, &error) != true;
                break;
            }
            case 'F':
            {
                NSError *localError = nil;

                CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];

                CDPFollowUpContext *context = [CDPFollowUpContext contextForRecoveryKeyRepair];
                context.force = true;

                secnotice("followup", "Posting a follow up (for SOS) of type recovery key");
                [cdpd postFollowUpWithContext:context error:&localError];
                if(localError){
                    printmsg(CFSTR("Request to CoreCDP to follow up failed: %@\n"), localError);
                } else {
                    printmsg(CFSTR("CoreCDP handling follow up\n"));
                }
                break;
            }
            case 'V': {
                NSError *localError = nil;
                NSString *testString = [NSString stringWithUTF8String:optarg];
                NSString *fileName = [NSString stringWithFormat:@"%@.plist", testString];
                if(testString == nil)
                    return SHOW_USAGE_MESSAGE;
                
                NSDictionary *ver = SecRKCopyAccountRecoveryVerifier(testString, &localError);
                if(ver == nil) {
                    printmsg(CFSTR("Failed to make verifier dictionary: %@\n"), localError);
                    return SHOW_USAGE_MESSAGE;
                }
                
                printmsg(CFSTR("Verifier Dictionary: %@\n\n"), ver);
                printmsg(CFSTR("Writing plist to %@\n"), (__bridge CFStringRef) fileName);

                [ver writeToFile:fileName atomically:YES];

                }
                break;

            case '?':
            default:
            {
                printf("%s [...options]\n", getprogname());
                for (unsigned n = 0; n < sizeof(long_options)/sizeof(long_options[0]); n++) {
                    printf("\t [-%c|--%s\n", long_options[n].val, long_options[n].name);
                }
                return SHOW_USAGE_MESSAGE;
            }
        }
    if (hadError)
        printerr(CFSTR("Error: %@\n"), error);

    return result;
}
