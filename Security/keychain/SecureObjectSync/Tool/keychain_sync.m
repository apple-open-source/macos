/*
 * Copyright (c) 2003-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 *
 * keychain_add.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>
#include <readpassphrase.h>

#include <Security/SecItem.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/securityd/SOSCloudCircleServer.h"
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecOTRSession.h>
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "SecurityTool/sharedTool/readline.h"
#include <notify.h>

#include "keychain_sync.h"
#include "keychain_log.h"
#include "syncbackup.h"

#include "secToolFileIO.h"
#include "secViewDisplay.h"
#include "accountCirclesViewsPrint.h"

#include <Security/SecPasswordGenerate.h>

#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18


static bool clearAllKVS(CFErrorRef *error)
{
    __block bool result = false;
    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    secnotice("circleOps", "security tool called SOSCloudKeychainClearAll to clear KVS");
    SOSCloudKeychainClearAll(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef cerror)
    {
        result = (cerror != NULL);
        dispatch_semaphore_signal(waitSemaphore);
    });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);

    return result;
}

static bool enableDefaultViews()
{
    bool result = false;
    CFMutableSetRef viewsToEnable = SOSViewCopyViewSet(kViewSetV0);
    CFMutableSetRef viewsToDisable = CFSetCreateMutable(NULL, 0, NULL);
    
    result = SOSCCViewSet(viewsToEnable, viewsToDisable);
    CFRelease(viewsToEnable);
    CFRelease(viewsToDisable);
    return result;
}

static bool requestToJoinCircle(CFErrorRef *error)
{
    // Set the visual state of switch based on membership in circle
    bool hadError = false;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(error);
    
    switch (ccstatus)
    {
    case kSOSCCCircleAbsent:
        hadError = !SOSCCResetToOffering(error);
        hadError &= enableDefaultViews();
        break;
    case kSOSCCNotInCircle:
        hadError = !SOSCCRequestToJoinCircle(error);
        hadError &= enableDefaultViews();
        break;
    default:
        printerr(CFSTR("Request to join circle with bad status:  %@ (%d)\n"), SOSCCGetStatusDescription(ccstatus), ccstatus);
        break;
    }
    return hadError;
}

static bool setPassword(char *labelAndPassword, CFErrorRef *err)
{
    char *last = NULL;
    char *token0 = strtok_r(labelAndPassword, ":", &last);
    char *token1 = strtok_r(NULL, "", &last);
    CFStringRef label = token1 ? CFStringCreateWithCString(NULL, token0, kCFStringEncodingUTF8) : CFSTR("security command line tool");
    char *password_token = token1 ? token1 : token0;
    password_token = password_token ? password_token : "";
    CFDataRef password = CFDataCreate(NULL, (const UInt8*) password_token, strlen(password_token));
    bool returned = !SOSCCSetUserCredentials(label, password, err);
    CFRelease(label);
    CFRelease(password);
    return returned;
}

static bool tryPassword(char *labelAndPassword, CFErrorRef *err)
{
    char *last = NULL;
    char *token0 = strtok_r(labelAndPassword, ":", &last);
    char *token1 = strtok_r(NULL, "", &last);
    CFStringRef label = token1 ? CFStringCreateWithCString(NULL, token0, kCFStringEncodingUTF8) : CFSTR("security command line tool");
    char *password_token = token1 ? token1 : token0;
    password_token = password_token ? password_token : "";
    CFDataRef password = CFDataCreate(NULL, (const UInt8*) password_token, strlen(password_token));
    bool returned = !SOSCCTryUserCredentials(label, password, err);
    CFRelease(label);
    CFRelease(password);
    return returned;
}

/*
 * Prompt user, call SOSCCTryUserCredentials.
 * Does not support optional label syntax like -T/-P.
 * Returns true on success.
 */
static bool
promptAndTryPassword(CFErrorRef *error)
{
    bool success = false;
    char passbuf[1024];
    CFDataRef password;

    if (readpassphrase("iCloud password: ", passbuf, sizeof(passbuf), RPP_REQUIRE_TTY) != NULL) {
        password = CFDataCreate(NULL, (const UInt8 *)passbuf, strlen(passbuf));
        if (password != NULL) {
            success = SOSCCTryUserCredentials(CFSTR("security command line tool"), password, error);
            CFReleaseNull(password);
        }
    }

    return success;
}

static bool syncAndWait(CFErrorRef *err)
{
    __block CFTypeRef objects = NULL;

    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);

    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secinfo("sync", "SOSCloudKeychainSynchronizeAndWait returned: %@", returnedValues);
        if (error)
            secerror("SOSCloudKeychainSynchronizeAndWait returned error: %@", error);
        objects = CFRetainSafe(returnedValues);

        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", objects);
        dispatch_semaphore_signal(waitSemaphore);
    };

    SOSCloudKeychainSynchronizeAndWait(generalq, replyBlock);

	dispatch_semaphore_wait(waitSemaphore, finishTime);

    (void)SOSCCDumpCircleKVSInformation(NULL);
    fprintf(outFile, "\n");
    return false;
}

static void dumpStringSet(CFStringRef label, CFSetRef s) {
    if(!s || !label) return;
    
    printmsg(CFSTR("%@: { "), label);
    __block bool first = true;
    CFSetForEach(s, ^(const void *p) {
        CFStringRef fmt = CFSTR(", %@");
        if(first) {
            fmt = CFSTR("%@");
        }
        CFStringRef string = (CFStringRef) p;
        printmsg(fmt, string);
        first=false;
    });
    printmsg(CFSTR(" }\n"), NULL);
}

static bool dumpMyPeer(CFErrorRef *error) {
    SOSPeerInfoRef myPeer = SOSCCCopyMyPeerInfo(error);

    if (!myPeer) return false;
    
    CFStringRef peerID = SOSPeerInfoGetPeerID(myPeer);
    CFStringRef peerName = SOSPeerInfoGetPeerName(myPeer);
    CFIndex peerVersion = SOSPeerInfoGetVersion(myPeer);
    bool retirement = SOSPeerInfoIsRetirementTicket(myPeer);
    
    printmsg(CFSTR("Peer Name: %@    PeerID: %@   Version: %d\n"), peerName, peerID, peerVersion);
    if(retirement) {
        CFDateRef retdate = SOSPeerInfoGetRetirementDate(myPeer);
        printmsg(CFSTR("Retired: %@\n"), retdate);

    }
    
    if(peerVersion >= 2) {
        CFMutableSetRef views = SOSPeerInfoV2DictionaryCopySet(myPeer, sViewsKey);
        CFStringRef serialNumber = SOSPeerInfoV2DictionaryCopyString(myPeer, sSerialNumberKey);
        CFBooleanRef preferIDS = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDS);
        CFBooleanRef preferIDSFragmentation = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDSFragmentation);
        CFBooleanRef preferIDSACKModel = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDSACKModel);
        CFStringRef transportType = SOSPeerInfoV2DictionaryCopyString(myPeer, sTransportType);
        CFStringRef idsDeviceID = SOSPeerInfoV2DictionaryCopyString(myPeer, sDeviceID);

        printmsg(CFSTR("Serial#: %@   PrefIDS#: %@   PrefFragmentation#: %@   PrefACK#: %@   transportType#: %@   idsDeviceID#: %@\n"),
                 serialNumber, preferIDS, preferIDSFragmentation, preferIDSACKModel, transportType, idsDeviceID);

        printmsg(CFSTR("Serial#: %@\n"),
                 serialNumber);
        dumpStringSet(CFSTR("             Views: "), views);


        CFReleaseSafe(serialNumber);
        CFReleaseSafe(preferIDS);
        CFReleaseSafe(preferIDSFragmentation);
        CFReleaseSafe(views);
        CFReleaseSafe(transportType);
        CFReleaseSafe(idsDeviceID);
    }

    bool ret = myPeer != NULL;
    CFReleaseNull(myPeer);
    return ret;
}

static bool setBag(char *itemName, CFErrorRef *err)
{
    __block bool success = false;
    __block CFErrorRef error = NULL;

    CFStringRef random = SecPasswordCreateWithRandomDigits(10, NULL);

    CFStringPerformWithUTF8CFData(random, ^(CFDataRef stringAsData) {
        if (0 == strncasecmp(optarg, "single", 6) || 0 == strncasecmp(optarg, "all", 3)) {
            bool includeV0 = (0 == strncasecmp(optarg, "all", 3));
            printmsg(CFSTR("Setting iCSC single using entropy from string: %@\n"), random);
            CFDataRef aks_bag = SecAKSCopyBackupBagWithSecret(CFDataGetLength(stringAsData), (uint8_t*)CFDataGetBytePtr(stringAsData), &error);

            if (aks_bag) {
                success = SOSCCRegisterSingleRecoverySecret(aks_bag, includeV0, &error);
                if (!success) {
                    printmsg(CFSTR("Failed registering single secret %@"), error);
                    CFReleaseNull(aks_bag);
                }
            } else {
                printmsg(CFSTR("Failed to create aks_bag: %@"), error);
            }
            CFReleaseNull(aks_bag);
        } else if (0 == strncasecmp(optarg, "device", 6)) {
            printmsg(CFSTR("Setting Device Secret using entropy from string: %@\n"), random);

            SOSPeerInfoRef me = SOSCCCopyMyPeerWithNewDeviceRecoverySecret(stringAsData, &error);

            success = me != NULL;

            if (!success)
                printmsg(CFSTR("Failed: %@\n"), err);
            CFReleaseNull(me);
        } else {
            printmsg(CFSTR("Unrecognized argument to -b %s\n"), optarg);
        }
    });


    return success;
}

static void prClientViewState(char *label, bool result) {
    fprintf(outFile, "Sync Status for %s: %s\n", label, (result) ? "enabled": "not enabled");
}

static bool clientViewStatus(CFErrorRef *error) {
    prClientViewState("KeychainV0", SOSCCIsIcloudKeychainSyncing());
    prClientViewState("Safari", SOSCCIsSafariSyncing());
    prClientViewState("AppleTV", SOSCCIsAppleTVSyncing());
    prClientViewState("HomeKit", SOSCCIsHomeKitSyncing());
    prClientViewState("Wifi", SOSCCIsWiFiSyncing());
    prClientViewState("AlwaysOnNoInitialSync", SOSCCIsContinuityUnlockSyncing());
    return false;
}


static bool dumpYetToSync(CFErrorRef *error) {
    CFArrayRef yetToSyncViews = SOSCCCopyYetToSyncViewsList(error);

    bool hadError = yetToSyncViews;

    if (yetToSyncViews) {
        __block CFStringRef separator = CFSTR("");

        printmsg(CFSTR("Yet to sync views: ["), NULL);

        CFArrayForEach(yetToSyncViews, ^(const void *value) {
            if (isString(value)) {
                printmsg(CFSTR("%@%@"), separator, value);

                separator = CFSTR(", ");
            }
        });
        printmsg(CFSTR("]\n"), NULL);
    }

    return !hadError;
}

#pragma mark -
#pragma mark --remove-peer

static void
add_matching_peerinfos(CFMutableArrayRef list, CFArrayRef spids, CFArrayRef (*copy_peer_func)(CFErrorRef *))
{
    CFErrorRef error;
    CFArrayRef peers;
    SOSPeerInfoRef pi;
    CFStringRef spid;
    CFIndex i, j;

    peers = copy_peer_func(&error);
    if (peers != NULL) {
        for (i = 0; i < CFArrayGetCount(peers); i++) {
            pi = (SOSPeerInfoRef)CFArrayGetValueAtIndex(peers, i);
            for (j = 0; j < CFArrayGetCount(spids); j++) {
                spid = (CFStringRef)CFArrayGetValueAtIndex(spids, j);
                if (CFStringGetLength(spid) < 8) {
                    continue;
                }
                if (CFStringHasPrefix(SOSPeerInfoGetPeerID(pi), spid)) {
                    CFArrayAppendValue(list, pi);
                }
            }
        }
        CFRelease(peers);
    } else {
        // unlikely
        CFShow(error);
        CFRelease(error);
    }
}

static CFArrayRef
copy_peerinfos(CFArrayRef spids)
{
    CFMutableArrayRef matches;

    matches = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    add_matching_peerinfos(matches, spids, SOSCCCopyValidPeerPeerInfo);
    add_matching_peerinfos(matches, spids, SOSCCCopyNotValidPeerPeerInfo);
    add_matching_peerinfos(matches, spids, SOSCCCopyRetirementPeerInfo);

    return matches;
}

static bool
doRemovePeers(CFArrayRef peerids, CFErrorRef *error)
{
    bool success = false;
    CFArrayRef peers = NULL;
    CFErrorRef localError = NULL;
    CFIndex i;
    char buf[16];

    peers = copy_peerinfos(peerids);
    if (peers == NULL || CFArrayGetCount(peers) == 0) {
        fprintf(stdout, "No matching peers to remove.\n");
        success = true;
        goto done;
    }

    fprintf(stdout, "Matched the following devices:\n");
    for (i = 0; i < CFArrayGetCount(peers); i++) {
        // Ugly.
        CFShow(CFArrayGetValueAtIndex(peers, i));
    }

    if (readpassphrase("Confirm removal (y/N): ", buf, sizeof(buf), RPP_ECHO_ON | RPP_FORCEUPPER) == NULL) {
        goto done;
    }

    if (buf[0] != 'Y') {
        success = true;
        goto done;
    }

    success = SOSCCRemovePeersFromCircle(peers, &localError);
    if (!success && isSOSErrorCoded(localError, kSOSErrorPrivateKeyAbsent)) {
        CFReleaseNull(localError);

        success = promptAndTryPassword(&localError);
        if (success) {
            success = SOSCCRemovePeersFromCircle(peers, &localError);
        }
    }

done:
    CFReleaseNull(peers);

    if (!success && error != NULL) {
        *error = localError;
    } else {
        CFReleaseNull(localError);
    }

    return success;
}

#pragma mark -

// enable, disable, accept, reject, status, Reset, Clear
int
keychain_sync(int argc, char * const *argv)
{
    /*
	 "Keychain Syncing"
	 "    -d     disable"
	 "    -e     enable (join/create circle)"
	 "    -i     info (current status)"
	 "    -m     dump my peer"
	 "
	 "Account/Circle Management"
	 "    -a     accept all applicants"
	 "    -l     [reason] sign out of circle + set custom departure reason"
	 "    -q     sign out of circle"
	 "    -r     reject all applicants"
	 "    -E     ensure fresh parameters"
     "    -b     device|all|single Register a backup bag - THIS RESETS BACKUPS!\n"
     "    -A     Apply to a ring\n"
     "    -B     Withdrawl from a ring\n"
     "    -G     Enable Ring\n"
     "    -F     Ring Status\n"
     "    -I     Dump Ring Information\n"

	 "    -N     (re-)set to new account (USE WITH CARE: device will not leave circle before resetting account!)"
	 "    -O     reset to offering"
	 "    -R     reset circle"
	 "    -X     [limit]  best effort bail from circle in limit seconds"
     "    -o     list view unaware peers in circle"
     "    -0     boot view unaware peers from circle"
     "    -1     grab account state from the keychain"
     "    -2     delete account state from the keychain"
     "    -3     grab engine state from the keychain"
     "    -4     delete engine state from the keychain"
     "    -5     cleanup old KVS keys in KVS"
     "    -6     [test]populate KVS with garbage KVS keys
	 "
	 "Password"
	 "    -P     [label:]password  set password (optionally for a given label) for sync"
	 "    -T     [label:]password  try password (optionally for a given label) for sync"
	 "
	 "KVS"
	 "    -k     pend all registered kvs keys"
	 "    -C     clear all values from KVS"
	 "    -D     [itemName]  dump contents of KVS"
     "    -W     sync and dump"
	 "
	 "Misc"
	 "    -v     [enable|disable|query:viewname] enable, disable, or query my PeerInfo's view set"
	 "             viewnames are: keychain|masterkey|iclouddrive|photos|cloudkit|escrow|fde|maildrop|icloudbackup|notes|imessage|appletv|homekit|"
     "                            wifi|passwords|creditcards|icloudidentity|othersyncable"
     "    -L     list all known view and their status"
	 "    -U     purge private key material cache\n"
     "    -V     Report View Sync Status on all known clients.\n"
     "    -H     Set escrow record.\n"
     "    -J     Get the escrow record.\n"
     "    -M     Check peer availability.\n"
     */
    enum {
        SYNC_REMOVE_PEER,
    };
    int action = -1;
    const struct option longopts[] = {
        { "remove-peer",    required_argument,  &action,    SYNC_REMOVE_PEER, },
        { NULL,             0,                  NULL,       0, },
    };
    int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;
    CFMutableArrayRef peers2remove = NULL;
    SOSLogSetOutputTo(NULL, NULL);

    while ((ch = getopt_long(argc, argv, "ab:deg:hikl:mopq:rSv:w:x:zA:B:MNJCDEF:HG:ILOP:RT:UWX:VY0123456", longopts, NULL)) != -1)
        switch  (ch) {
		case 'l':
		{
			fprintf(outFile, "Signing out of circle\n");
			hadError = !SOSCCSignedOut(true, &error);
			if (!hadError) {
				errno = 0;
				int reason = (int) strtoul(optarg, NULL, 10);
				if (errno != 0 ||
					reason < kSOSDepartureReasonError ||
					reason >= kSOSNumDepartureReasons) {
					fprintf(errFile, "Invalid custom departure reason %s\n", optarg);
				} else {
					fprintf(outFile, "Setting custom departure reason %d\n", reason);
					hadError = !SOSCCSetLastDepartureReason(reason, &error);
					notify_post(kSOSCCCircleChangedNotification);
				}
			}
			break;
		}
			
		case 'q':
		{
			fprintf(outFile, "Signing out of circle\n");
			bool signOutImmediately = false;
			if (strcasecmp(optarg, "true") == 0) {
				signOutImmediately = true;
			} else if (strcasecmp(optarg, "false") == 0) {
				signOutImmediately = false;
			} else {
				fprintf(outFile, "Please provide a \"true\" or \"false\" whether you'd like to leave the circle immediately\n");
			}
			hadError = !SOSCCSignedOut(signOutImmediately, &error);
			notify_post(kSOSCCCircleChangedNotification);
			break;
		}
		case 'e':
			fprintf(outFile, "Turning ON keychain syncing\n");
			hadError = requestToJoinCircle(&error);
			break;
			
		case 'd':
			fprintf(outFile, "Turning OFF keychain syncing\n");
			hadError = !SOSCCRemoveThisDeviceFromCircle(&error);
			break;
			
		case 'a':
		{
			CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
			if (applicants) {
				hadError = !SOSCCAcceptApplicants(applicants, &error);
				CFRelease(applicants);
			} else {
				fprintf(errFile, "No applicants to accept\n");
			}
			break;
		}
			
		case 'r':
		{
			CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
			if (applicants)	{
				hadError = !SOSCCRejectApplicants(applicants, &error);
				CFRelease(applicants);
			} else {
				fprintf(errFile, "No applicants to reject\n");
			}
			break;
		}
			
		case 'i':
			SOSCCDumpCircleInformation();
			SOSCCDumpEngineInformation();
			break;
			
		case 'k':
			notify_post("com.apple.security.cloudkeychain.forceupdate");
			break;

        case 'o':
        {
            SOSCCDumpViewUnwarePeers();
            break;
        }

        case '0':
        {
            CFArrayRef unawares = SOSCCCopyViewUnawarePeerInfo(&error);
            if (unawares) {
                hadError = !SOSCCRemovePeersFromCircle(unawares, &error);
            } else {
                hadError = true;
            }
            CFReleaseNull(unawares);
            break;
        }
        case '1':
        {
            CFDataRef accountState = SOSCCCopyAccountState(&error);
            if (accountState) {
                printmsg(CFSTR(" %@\n"), CFDataCopyHexString(accountState));
            } else {
                hadError = true;
            }
            CFReleaseNull(accountState);
            break;
        }
        case '2':
        {
            bool status = SOSCCDeleteAccountState(&error);
            if (status) {
                printmsg(CFSTR("Deleted account from the keychain %d\n"), status);
            } else {
                hadError = true;
            }
            break;
        }
        case '3':
        {
            CFDataRef engineState = SOSCCCopyEngineData(&error);
            if (engineState) {
                printmsg(CFSTR(" %@\n"), CFDataCopyHexString(engineState));
            } else {
                hadError = true;
            }
            CFReleaseNull(engineState);
            break;
        }
        case '4':
        {
            bool status = SOSCCDeleteEngineState(&error);
            if (status) {
                printmsg(CFSTR("Deleted engine-state from the keychain %d\n"), status);
            } else {
                hadError = true;
            }
            break;
        }
        case '5' :
        {
            bool result = SOSCCCleanupKVSKeys(&error);
            if(result)
            {
                printmsg(CFSTR("Got all the keys from KVS %d\n"), result);
            }else {
                hadError = true;
            }
            break;
        }
        case '6' :
        {
            bool result = SOSCCTestPopulateKVSWithBadKeys(&error);
            if(result)
                {
                    printmsg(CFSTR("Populated KVS with garbage %d\n"), result);
                }else {
                    hadError = true;
                }
                break;
        }
		case 'E':
		{
			fprintf(outFile, "Ensuring Fresh Parameters\n");
			bool result = SOSCCRequestEnsureFreshParameters(&error);
			if (error) {
				hadError = true;
				break;
			}
			if (result) {
				fprintf(outFile, "Refreshed Parameters Ensured!\n");
			} else {
				fprintf(outFile, "Problem trying to ensure fresh parameters\n");
			}
			break;
		}
		case 'A':
		{
			fprintf(outFile, "Applying to Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCApplyToARing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'B':
		{
			fprintf(outFile, "Withdrawing from Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCWithdrawlFromARing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'F':
		{
			fprintf(outFile, "Status of this device in the Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCRingStatus(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'G':
		{
			fprintf(outFile, "Enabling Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCEnableRing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
        case 'H':
        {
            fprintf(outFile, "Setting random escrow record\n");
            bool success = SOSCCSetEscrowRecord(CFSTR("label"), 8, &error);
            if(success)
                hadError = false;
            else
                hadError = true;
            break;
        }
        case 'J':
        {
            CFDictionaryRef attempts = SOSCCCopyEscrowRecord(&error);
            if(attempts){
                CFDictionaryForEach(attempts, ^(const void *key, const void *value) {
                    if(isString(key)){
                        char *keyString = CFStringToCString(key);
                        fprintf(outFile, "%s:\n", keyString);
                        free(keyString);
                    }
                    if(isDictionary(value)){
                        CFDictionaryForEach(value, ^(const void *key, const void *value) {
                            if(isString(key)){
                                char *keyString = CFStringToCString(key);
                                fprintf(outFile, "%s: ", keyString);
                                free(keyString);
                            }
                            if(isString(value)){
                                char *time = CFStringToCString(value);
                                fprintf(outFile, "timestamp: %s\n", time);
                                free(time);
                            }
                            else if(isNumber(value)){
                                uint64_t tries;
                                CFNumberGetValue(value, kCFNumberLongLongType, &tries);
                                fprintf(outFile, "date: %llu\n", tries);
                            }
                        });
                    }
                  
               });
            }
            CFReleaseNull(attempts);
            hadError = false;
            break;
        }
		case 'I':
		{
			fprintf(outFile, "Printing all the rings\n");
			CFStringRef ringdescription = SOSCCGetAllTheRings(&error);
			if(!ringdescription)
				hadError = true;
			else
				fprintf(outFile, "Rings: %s", CFStringToCString(ringdescription));
			
			break;
		}

		case 'N':
			hadError = !SOSCCAccountSetToNew(&error);
			if (!hadError)
				notify_post(kSOSCCCircleChangedNotification);
			break;

		case 'R':
			hadError = !SOSCCResetToEmpty(&error);
			break;

		case 'O':
			hadError = !SOSCCResetToOffering(&error);
			break;

		case 'm':
			hadError = !dumpMyPeer(&error);
			break;

		case 'C':
			hadError = clearAllKVS(&error);
			break;

		case 'P':
			hadError = setPassword(optarg, &error);
			break;

		case 'T':
			hadError = tryPassword(optarg, &error);
			break;

		case 'X':
		{
			uint64_t limit = strtoul(optarg, NULL, 10);
			hadError = !SOSCCBailFromCircle_BestEffort(limit, &error);
			break;
		}

		case 'U':
			hadError = !SOSCCPurgeUserCredentials(&error);
			break;

		case 'D':
			(void)SOSCCDumpCircleKVSInformation(optarg);
			break;

		case 'W':
			hadError = syncAndWait(&error);
			break;

		case 'v':
			hadError = !viewcmd(optarg, &error);
			break;

        case 'V':
            hadError = clientViewStatus(&error);
            break;
        case 'L':
            hadError = !listviewcmd(&error);
            break;

        case 'b':
            hadError = setBag(optarg, &error);
            break;

        case 'Y':
            hadError = dumpYetToSync(&error);
            break;
        case 0:
            switch (action) {
            case SYNC_REMOVE_PEER: {
                CFStringRef optstr;
                optstr = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
                if (peers2remove == NULL) {
                    peers2remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
                }
                CFArrayAppendValue(peers2remove, optstr);
                CFRelease(optstr);
                break;
            }
            default:
                return SHOW_USAGE_MESSAGE;
            }
            break;
        case '?':
        default:
            return SHOW_USAGE_MESSAGE;
    }

    if (peers2remove != NULL) {
        hadError = !doRemovePeers(peers2remove, &error);
        CFRelease(peers2remove);
    }

    if (hadError) {
        printerr(CFSTR("Error: %@\n"), error);
    }

    return result;
}
