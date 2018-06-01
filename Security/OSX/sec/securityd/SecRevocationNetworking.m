/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <CFNetwork/CFNSURLConnection.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <os/transaction_private.h>
#include "utilities/debugging.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/SecPLWrappers.h"
#include "utilities/SecFileLocations.h"

#include "SecRevocationDb.h"
#import "SecTrustLoggingServer.h"

#import "SecRevocationNetworking.h"

#define kSecRevocationBasePath          "/Library/Keychains/crls"

static CFStringRef kSecPrefsDomain      = CFSTR("com.apple.security");
static CFStringRef kUpdateWiFiOnlyKey   = CFSTR("ValidUpdateWiFiOnly");
static CFStringRef kUpdateBackgroundKey = CFSTR("ValidUpdateBackground");

extern CFAbsoluteTime gUpdateStarted;
extern CFAbsoluteTime gNextUpdate;

static int checkBasePath(const char *basePath) {
    return mkpath_np((char*)basePath, 0755);
}

static uint64_t systemUptimeInSeconds() {
    struct timeval boottime;
    size_t tv_size = sizeof(boottime);
    time_t now, uptime = 0;
    int mib[2];
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
    (void) time(&now);
    if (sysctl(mib, 2, &boottime, &tv_size, NULL, 0) != -1 &&
        boottime.tv_sec != 0) {
        uptime = now - boottime.tv_sec;
    }
    return (uint64_t)uptime;
}

typedef void (^CompletionHandler)(void);

@interface ValidDelegate : NSObject <NSURLSessionDelegate, NSURLSessionTaskDelegate, NSURLSessionDataDelegate>
@property CompletionHandler handler;
@property dispatch_queue_t revDbUpdateQueue;
@property os_transaction_t transaction;
@property NSString *currentUpdateServer;
@property NSFileHandle *currentUpdateFile;
@property NSURL *currentUpdateFileURL;
@property BOOL finishedDownloading;
@end

@implementation ValidDelegate

- (void)reschedule {
    /* make sure we release any os transaction, if we went active */
    if (self->_transaction) {
        //os_release(self->_transaction); // ARC does this for us and won't let us call release
        self->_transaction = NULL;
    }
    /* POWER LOG EVENT: operation canceled */
    SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
        @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
        @"event" : (self->_finishedDownloading) ? @"updateCanceled" : @"downloadCanceled"
    });
    secnotice("validupdate", "%s canceled at %f",
        (self->_finishedDownloading) ? "update" : "download",
        (double)CFAbsoluteTimeGetCurrent());

    self->_handler();
    SecRevocationDbComputeAndSetNextUpdateTime();
}

- (void)updateDb:(NSUInteger)version {
    __block NSURL *updateFileURL = self->_currentUpdateFileURL;
    __block NSString *updateServer = self->_currentUpdateServer;
    __block NSFileHandle *updateFile = self->_currentUpdateFile;
    if (!updateFileURL || !updateFile) {
        [self reschedule];
        return;
    }

    dispatch_async(_revDbUpdateQueue, ^{
        /* POWER LOG EVENT: background update started */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"updateStarted"
        });
        secnotice("validupdate", "update started at %f", (double)CFAbsoluteTimeGetCurrent());

        CFDataRef updateData = NULL;
        const char *updateFilePath = [updateFileURL fileSystemRepresentation];
        int rtn;
        if ((rtn = readValidFile(updateFilePath, &updateData)) != 0) {
            secerror("failed to read %@ with error %d", updateFileURL, rtn);
            TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TAFatalError, rtn);
            [self reschedule];
            return;
        }

        secdebug("validupdate", "verifying and ingesting data from %@", updateFileURL);
        SecValidUpdateVerifyAndIngest(updateData, (__bridge CFStringRef)updateServer, (0 == version));
        if ((rtn = munmap((void *)CFDataGetBytePtr(updateData), CFDataGetLength(updateData))) != 0) {
            secerror("unable to unmap current update %ld bytes at %p (error %d)", CFDataGetLength(updateData), CFDataGetBytePtr(updateData), rtn);
        }
        CFReleaseNull(updateData);

        /* We're done with this file */
        [updateFile closeFile];
        if (updateFilePath) {
            (void)remove(updateFilePath);
        }
        self->_currentUpdateFile = nil;
        self->_currentUpdateFileURL = nil;
        self->_currentUpdateServer = nil;

        /* POWER LOG EVENT: background update finished */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"updateFinished"
        });

        /* Update is complete */
        secnotice("validupdate", "update finished at %f", (double)CFAbsoluteTimeGetCurrent());
        gUpdateStarted = 0;

        self->_handler();
    });
}

- (NSInteger)versionFromTask:(NSURLSessionTask *)task {
    return atol([task.taskDescription cStringUsingEncoding:NSUTF8StringEncoding]);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
didReceiveResponse:(NSURLResponse *)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler {
    secinfo("validupdate", "Session %@ data task %@ returned response %ld, expecting %lld bytes", session, dataTask,
            (long)[(NSHTTPURLResponse *)response statusCode],[response expectedContentLength]);

    (void)checkBasePath(kSecRevocationBasePath);
    CFURLRef updateFileURL = SecCopyURLForFileInRevocationInfoDirectory(CFSTR("update-current"));
    self->_currentUpdateFileURL = (updateFileURL) ? CFBridgingRelease(updateFileURL) : nil;
    const char *updateFilePath = [self->_currentUpdateFileURL fileSystemRepresentation];
    if (!updateFilePath) {
        secnotice("validupdate", "failed to find revocation info directory. canceling task %@", dataTask);
        completionHandler(NSURLSessionResponseCancel);
        [self reschedule];
        return;
    }

    /* Clean up any old files from previous tasks. */
    (void)remove(updateFilePath);

    int fd;
    off_t off;
    fd = open(updateFilePath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0  || (off = lseek(fd, 0, SEEK_SET)) < 0) {
        secnotice("validupdate","unable to open %@ (errno %d)", self->_currentUpdateFileURL, errno);
    }
    if (fd >= 0) {
        close(fd);
    }

    /* POWER LOG EVENT: background download actually started */
    SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
        @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
        @"event" : @"downloadStarted"
    });
    secnotice("validupdate", "download started at %f", (double)CFAbsoluteTimeGetCurrent());

    NSError *error = nil;
    self->_currentUpdateFile = [NSFileHandle fileHandleForWritingToURL:self->_currentUpdateFileURL error:&error];
    if (!self->_currentUpdateFile) {
        secnotice("validupdate", "failed to open %@: %@. canceling task %@", self->_currentUpdateFileURL, error, dataTask);
#if ENABLE_TRUSTD_ANALYTICS
        [[TrustdHealthAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventValidUpdate hardFailure:NO result:error];
#endif // ENABLE_TRUSTD_ANALYTICS
        completionHandler(NSURLSessionResponseCancel);
        [self reschedule];
        return;
    }

    /* We're about to begin downloading -- go active now so we don't get jetsammed */
    self->_transaction = os_transaction_create("com.apple.trustd.valid.download");
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {
    secdebug("validupdate", "Session %@ data task %@ returned %lu bytes (%lld bytes so far) out of expected %lld bytes",
             session, dataTask, (unsigned long)[data length], [dataTask countOfBytesReceived], [dataTask countOfBytesExpectedToReceive]);

    if (!self->_currentUpdateFile) {
        secnotice("validupdate", "received data, but output file is not open");
        [dataTask cancel];
        [self reschedule];
        return;
    }

    @try {
        /* Writing can fail and throw an exception, e.g. if we run out of disk space. */
        [self->_currentUpdateFile writeData:data];
    }
    @catch(NSException *exception) {
        secnotice("validupdate", "%s", exception.description.UTF8String);
        TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TARecoverableError, errSecDiskFull);
        [dataTask cancel];
        [self reschedule];
    }
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error {
    /* all finished downloading data -- go inactive */
    if (self->_transaction) {
        // os_release(self->_transaction); // ARC does this for us and won't let us call release
        self->_transaction = NULL;
    }
    if (error) {
        secnotice("validupdate", "Session %@ task %@ failed with error %@", session, task, error);
#if ENABLE_TRUSTD_ANALYTICS
        [[TrustdHealthAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventValidUpdate hardFailure:NO result:error];
#endif // ENABLE_TRUSTD_ANALYTICS
        [self reschedule];
        /* close file before we leave */
        [self->_currentUpdateFile closeFile];
        self->_currentUpdateFile = nil;
        self->_currentUpdateServer = nil;
        self->_currentUpdateFileURL = nil;
    } else {
        /* POWER LOG EVENT: background download finished */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"downloadFinished"
        });
        secnotice("validupdate", "download finished at %f", (double)CFAbsoluteTimeGetCurrent());
        secdebug("validupdate", "Session %@ task %@ succeeded", session, task);
        self->_finishedDownloading = YES;
        [self updateDb:[self versionFromTask:task]];
    }
}

@end

@interface ValidUpdateRequest : NSObject
@property NSTimeInterval updateScheduled;
@property NSURLSession *backgroundSession;
@end

static ValidUpdateRequest *request = nil;

@implementation ValidUpdateRequest

- (NSURLSessionConfiguration *)validUpdateConfiguration {
    /* preferences to override defaults */
    CFTypeRef value = NULL;
    bool updateOnWiFiOnly = true;
    value = CFPreferencesCopyValue(kUpdateWiFiOnlyKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateOnWiFiOnly = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);
    bool updateInBackground = true;
    value = CFPreferencesCopyValue(kUpdateBackgroundKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateInBackground = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);

    NSURLSessionConfiguration *config = nil;
    if (updateInBackground) {
        config = [NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier: @"com.apple.trustd.networking.background"];
        config.networkServiceType = NSURLNetworkServiceTypeBackground;
        config.discretionary = YES;
    } else {
        config = [NSURLSessionConfiguration ephemeralSessionConfiguration]; // no cookies or data storage
        config.networkServiceType = NSURLNetworkServiceTypeDefault;
        config.discretionary = NO;
    }

    config.HTTPAdditionalHeaders = @{ @"User-Agent" : @"com.apple.trustd/2.0",
                                      @"Accept" : @"*/*",
                                      @"Accept-Encoding" : @"gzip,deflate,br"};

    config.TLSMinimumSupportedProtocol = kTLSProtocol12;
    config.TLSMaximumSupportedProtocol = kTLSProtocol13;

    config._requiresPowerPluggedIn = YES;

    config.allowsCellularAccess = (!updateOnWiFiOnly) ? YES : NO;

    return config;
}

- (void) createSession:(dispatch_queue_t)updateQueue forServer:(NSString *)updateServer {
    NSURLSessionConfiguration *config = [self validUpdateConfiguration];
    ValidDelegate *delegate = [[ValidDelegate alloc] init];
    delegate.handler = ^(void) {
        request.updateScheduled = 0.0;
        secdebug("validupdate", "resetting scheduled time");
    };
    delegate.transaction = NULL;
    delegate.revDbUpdateQueue = updateQueue;
    delegate.finishedDownloading = NO;
    delegate.currentUpdateServer = [updateServer copy];

    /* Callbacks should be on a separate NSOperationQueue.
       We'll then dispatch the work on updateQueue and return from the callback. */
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];
    _backgroundSession = [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:queue];
}

- (BOOL) scheduleUpdateFromServer:(NSString *)server forVersion:(NSUInteger)version withQueue:(dispatch_queue_t)updateQueue {
    if (!server) {
        secnotice("validupdate", "invalid update request");
        return NO;
    }

    if (!updateQueue) {
        secnotice("validupdate", "missing update queue, skipping update");
        return NO;
    }

    /* nsurlsessiond waits for unlock to finish launching, so we can't block trust evaluations
     * on scheduling this background task. Also, we want to wait a sufficient amount of time
     * after system boot before trying to initiate network activity, to avoid the possibility
     * of a performance regression in the boot path. */
    dispatch_async(updateQueue, ^{
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if (self.updateScheduled != 0.0) {
            secdebug("validupdate", "update in progress (scheduled %f)", (double)self.updateScheduled);
            return;
        } else {
            uint64_t uptime = systemUptimeInSeconds();
            const uint64_t minUptime = 180;
            if (uptime < minUptime) {
                gNextUpdate = now + (minUptime - uptime);
                gUpdateStarted = 0;
                secnotice("validupdate", "postponing update until %f", gNextUpdate);
            } else {
                self.updateScheduled = now;
                secnotice("validupdate", "scheduling update at %f", (double)self.updateScheduled);
            }
        }

        NSURL *validUrl = [NSURL URLWithString:[NSString stringWithFormat:@"https://%@/g3/v%ld",
                                                server, (unsigned long)version]];
        if (!validUrl) {
            secnotice("validupdate", "invalid update url");
            return;
        }

        /* clear all old sessions and cleanup disk (for previous download tasks) */
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            [NSURLSession _obliterateAllBackgroundSessionsWithCompletionHandler:^{
                secnotice("validupdate", "removing all old sessions for trustd");
            }];
        });

        if (!self.backgroundSession) {
            [self createSession:updateQueue forServer:server];
        }

        /* POWER LOG EVENT: scheduling our background download session now */
        SecPLLogRegisteredEvent(@"ValidUpdateEvent", @{
            @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
            @"event" : @"downloadScheduled",
            @"version" : @(version)
        });

        NSURLSessionDataTask *dataTask = [self.backgroundSession dataTaskWithURL:validUrl];
        dataTask.taskDescription = [NSString stringWithFormat:@"%lu",(unsigned long)version];
        [dataTask resume];
        secnotice("validupdate", "scheduled background data task %@ at %f", dataTask, CFAbsoluteTimeGetCurrent());
    });

    return YES;
}
@end

bool SecValidUpdateRequest(dispatch_queue_t queue, CFStringRef server, CFIndex version)  {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        request = [[ValidUpdateRequest alloc] init];
    });
    return [request scheduleUpdateFromServer:(__bridge NSString*)server forVersion:version withQueue:queue];
}
