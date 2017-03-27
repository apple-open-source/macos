/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

/*
 *  SecRevocationDb.c
 */

#include <securityd/SecRevocationDb.h>
#include <securityd/asynchttp.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCMS.h>
#include <Security/SecFramework.h>
#include <Security/SecInternal.h>
#include <AssertMacros.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dispatch/dispatch.h>
#include <asl.h>
#include "utilities/debugging.h"
#include "utilities/sqlutils.h"
#include "utilities/SecAppleAnchorPriv.h"
#include "utilities/iOSforOSX.h"
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>
#include <sqlite3.h>
#include <zlib.h>
#include <malloc/malloc.h>
#include <xpc/activity.h>
#include <xpc/private.h>

#include <CFNetwork/CFHTTPMessage.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFUtilities.h>


static CFStringRef kAcceptEncoding      = CFSTR("Accept-Encoding");
static CFStringRef kAppEncoding         = CFSTR("deflate");
static CFStringRef kUserAgent           = CFSTR("User-Agent");
static CFStringRef kAppUserAgent        = CFSTR("com.apple.trustd/1.0");
static CFStringRef kValidUpdateServer   = CFSTR("valid.apple.com");

static CFStringRef kSecPrefsDomain      = CFSTR("com.apple.security");
static CFStringRef kUpdateServerKey     = CFSTR("ValidUpdateServer");
static CFStringRef kUpdateEnabledKey    = CFSTR("ValidUpdateEnabled");
static CFStringRef kUpdateIntervalKey   = CFSTR("ValidUpdateInterval");
static CFStringRef kUpdateWiFiOnlyKey   = CFSTR("ValidUpdateWiFiOnly");

typedef CF_OPTIONS(CFOptionFlags, SecValidInfoFlags) {
    kSecValidInfoComplete               = 1u << 0,
    kSecValidInfoCheckOCSP              = 1u << 1,
    kSecValidInfoKnownOnly              = 1u << 2,
    kSecValidInfoRequireCT              = 1u << 3,
    kSecValidInfoAllowlist              = 1u << 4
};

/* minimum initial interval after process startup */
#define kSecMinUpdateInterval           (60.0 * 5)

/* second and subsequent intervals */
#define kSecStdUpdateInterval           (60.0 * 60)

/* maximum allowed interval */
#define kSecMaxUpdateInterval           (60.0 * 60 * 24 * 7)

/* background download timeout */
#define kSecMaxDownloadSeconds          (60.0 * 10)

#define kSecRevocationBasePath          "/Library/Keychains/crls"
#define kSecRevocationDbFileName        "valid.sqlite3"

bool SecRevocationDbVerifyUpdate(CFDictionaryRef update);
CFIndex SecRevocationDbIngestUpdate(CFDictionaryRef update);
void SecRevocationDbApplyUpdate(CFDictionaryRef update, CFIndex version);
CFAbsoluteTime SecRevocationDbComputeNextUpdateTime(CFDictionaryRef update);
void SecRevocationDbSetSchemaVersion(CFIndex dbversion);
void SecRevocationDbSetNextUpdateTime(CFAbsoluteTime nextUpdate);
CFAbsoluteTime SecRevocationDbGetNextUpdateTime(void);
dispatch_queue_t SecRevocationDbGetUpdateQueue(void);
void SecRevocationDbRemoveAllEntries(void);


static CFDataRef copyInflatedData(CFDataRef data) {
    if (!data) {
        return NULL;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    /* 32 is a magic value which enables automatic header detection
       of gzip or zlib compressed data. */
    if (inflateInit2(&zs, 32+MAX_WBITS) != Z_OK) {
        return NULL;
    }
    zs.next_in = (UInt8 *)(CFDataGetBytePtr(data));
    zs.avail_in = (uInt)CFDataGetLength(data);

    CFMutableDataRef outData = CFDataCreateMutable(NULL, 0);
    if (!outData) {
        return NULL;
    }
    CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);
    unsigned char *buf = malloc(buf_sz);
    int rc;
    do {
        zs.next_out = (Bytef*)buf;
        zs.avail_out = (uInt)buf_sz;
        rc = inflate(&zs, 0);
        CFIndex outLen = CFDataGetLength(outData);
        if (outLen < (CFIndex)zs.total_out) {
            CFDataAppendBytes(outData, (const UInt8*)buf, (CFIndex)zs.total_out - outLen);
        }
    } while (rc == Z_OK);

    inflateEnd(&zs);

    if (buf) {
        free(buf);
    }
    if (rc != Z_STREAM_END) {
        CFReleaseSafe(outData);
        return NULL;
    }
    return (CFDataRef)outData;
}

static CFDataRef copyDeflatedData(CFDataRef data) {
    if (!data) {
        return NULL;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        return NULL;
    }
    zs.next_in = (UInt8 *)(CFDataGetBytePtr(data));
    zs.avail_in = (uInt)CFDataGetLength(data);

    CFMutableDataRef outData = CFDataCreateMutable(NULL, 0);
    if (!outData) {
        return NULL;
    }
    CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);
    unsigned char *buf = malloc(buf_sz);
    int rc = Z_BUF_ERROR;
    do {
        zs.next_out = (Bytef*)buf;
        zs.avail_out = (uInt)buf_sz;
        rc = deflate(&zs, Z_FINISH);

        if (rc == Z_OK || rc == Z_STREAM_END) {
            CFIndex buf_used = buf_sz - zs.avail_out;
            CFDataAppendBytes(outData, (const UInt8*)buf, buf_used);
        }
        else if (rc == Z_BUF_ERROR) {
            free(buf);
            buf_sz = malloc_good_size(buf_sz * 2);
            buf = malloc(buf_sz);
            if (buf) {
                rc = Z_OK; /* try again with larger buffer */
            }
        }
    } while (rc == Z_OK && zs.avail_in);

    deflateEnd(&zs);

    if (buf) {
        free(buf);
    }
    if (rc != Z_STREAM_END) {
        CFReleaseSafe(outData);
        return NULL;
    }
    return (CFDataRef)outData;
}

static uint32_t calculateCrc32(CFDataRef data) {
    if (!data) { return 0; }
    uint32_t crc = (uint32_t)crc32(0L, Z_NULL, 0);
    uint32_t len = (uint32_t)CFDataGetLength(data);
    const unsigned char *bytes = CFDataGetBytePtr(data);
    return (uint32_t)crc32(crc, bytes, len);
}

static int checkBasePath(const char *basePath) {
    return mkpath_np((char*)basePath, 0755);
}

static int writeFile(const char          *fileName,
                     const unsigned char *bytes,    // compressed data, if crc != 0
                     size_t              numBytes,  // length of content to write
                     uint32_t            crc,       // crc32 over uncompressed content
                     uint32_t            length) {  // uncompressed content length
    int rtn, fd;
    off_t off;
    size_t numToWrite=numBytes;
    const unsigned char *p=bytes;

    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fd < 0) { return errno; }
    off = lseek(fd, 0, SEEK_SET);
    if(off < 0) { return errno; }
    if(crc) {
        /* add gzip header per RFC1952 2.2 */
        uint8_t hdr[10] = { 31, 139, 8, 0, 0, 0, 0, 0, 2, 3 };
        write(fd, hdr, sizeof(hdr));
        /* skip 2-byte stream header and 4-byte trailing CRC */
        if (numToWrite > 6) {
            numToWrite -= 6;
            p += 2;
        }
    }
    off = write(fd, p, numToWrite);
    if((size_t)off != numToWrite) {
        rtn = EIO;
    } else {
        rtn = 0;
    }
    if(crc) {
        /* add gzip trailer per RFC1952 2.2 */
        /* note: gzip seems to want these values in host byte order. */
        write(fd, &crc, sizeof(crc));
        write(fd, &length, sizeof(length));
    }
    close(fd);
    return rtn;
}

static int readFile(const char *fileName,
                    CFDataRef  *bytes) {   // allocated and returned
    int rtn, fd;
    char *buf;
    struct stat	sb;
    size_t size;
    ssize_t rrc;

    *bytes = NULL;
    fd = open(fileName, O_RDONLY);
    if(fd < 0) { return errno; }
    rtn = fstat(fd, &sb);
    if(rtn) { goto errOut; }
    if (sb.st_size > (off_t) ((UINT32_MAX >> 1)-1)) {
        rtn = EFBIG;
        goto errOut;
    }
    size = (size_t)sb.st_size;

    *bytes = (CFDataRef)CFDataCreateMutable(NULL, (CFIndex)size);
    if(!*bytes) {
        rtn = ENOMEM;
        goto errOut;
    }

    CFDataSetLength((CFMutableDataRef)*bytes, (CFIndex)size);
    buf = (char*)CFDataGetBytePtr(*bytes);
    rrc = read(fd, buf, size);
    if(rrc != (ssize_t) size) {
        rtn = EIO;
    }
    else {
        rtn = 0;
    }

errOut:
    close(fd);
    if(rtn) {
        CFReleaseNull(*bytes);
    }
    return rtn;
}

static bool isDbOwner() {
#if TARGET_OS_EMBEDDED
    if (getuid() == 64) // _securityd
#else
    if (getuid() == 0)
#endif
    {
        return true;
    }
    return false;
}


// MARK: -
// MARK: SecValidUpdateRequest

/* ======================================================================
   SecValidUpdateRequest
   ======================================================================*/

static CFAbsoluteTime gUpdateRequestScheduled = 0.0;
static CFAbsoluteTime gNextUpdate = 0.0;
static CFIndex gUpdateInterval = 0;
static CFIndex gLastVersion = 0;

typedef struct SecValidUpdateRequest *SecValidUpdateRequestRef;
struct SecValidUpdateRequest {
    asynchttp_t http;       /* Must be first field. */
    CFStringRef server;     /* Server name. (e.g. "valid.apple.com") */
    CFIndex version;        /* Our current version. */
    xpc_object_t criteria;  /* Constraints dictionary for request. */
};

static void SecValidUpdateRequestRelease(SecValidUpdateRequestRef request) {
    if (!request) {
        return;
    }
    CFReleaseSafe(request->server);
    asynchttp_free(&request->http);
    if (request->criteria) {
        xpc_release(request->criteria);
    }
    free(request);
}

static void SecValidUpdateRequestIssue(SecValidUpdateRequestRef request) {
    // issue the async http request now
    CFStringRef urlStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                  CFSTR("https://%@/get/v%ld"),
                                                  request->server, (long)request->version);

    CFURLRef url = (urlStr) ? CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL) : NULL;
    CFReleaseSafe(urlStr);
    if (!url) {
        secnotice("validupdate", "invalid update url");
        SecValidUpdateRequestRelease(request);
        return;
    }
    CFHTTPMessageRef msg = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
                                                      CFSTR("GET"), url, kCFHTTPVersion1_1);
    CFReleaseSafe(url);
    if (msg) {
        secdebug("validupdate", "%@", msg);
        CFHTTPMessageSetHeaderFieldValue(msg, CFSTR("Accept"), CFSTR("*/*"));
        CFHTTPMessageSetHeaderFieldValue(msg, kAcceptEncoding, kAppEncoding);
        CFHTTPMessageSetHeaderFieldValue(msg, kUserAgent, kAppUserAgent);
        bool done = asynchttp_request(msg, kSecMaxDownloadSeconds*NSEC_PER_SEC, &request->http);
        CFReleaseSafe(msg);
        if (done == false) {
            return;
        }
    }
    secdebug("validupdate", "no request issued");
    SecValidUpdateRequestRelease(request);
}

static bool SecValidUpdateRequestSchedule(SecValidUpdateRequestRef request) {
    if (!request || !request->server) {
        secnotice("validupdate", "invalid update request");
        SecValidUpdateRequestRelease(request);
        return false;
    } else if (gUpdateRequestScheduled != 0.0) {
        // TBD: may need a separate scheduled activity which can perform a request with
        // fewer constraints if our request has not been satisfied for a week or so
        secdebug("validupdate", "update request already scheduled at %f, will not reissue",
                 (double)gUpdateRequestScheduled);
        SecValidUpdateRequestRelease(request);
        return true; // request is still in the queue
    } else {
        gUpdateRequestScheduled = CFAbsoluteTimeGetCurrent();
        secdebug("validupdate", "scheduling update at %f", (double)gUpdateRequestScheduled);
    }

    // determine whether to issue request without waiting for activity criteria to be satisfied
    bool updateOnWiFiOnly = true;
    CFTypeRef value = (CFBooleanRef)CFPreferencesCopyValue(kUpdateWiFiOnlyKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateOnWiFiOnly = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);
    if (!updateOnWiFiOnly) {
        SecValidUpdateRequestIssue(request);
        gUpdateRequestScheduled = 0.0;
        return true;
    }

    xpc_object_t criteria = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_string(criteria, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_MAINTENANCE);
    // we want to start as soon as possible
    xpc_dictionary_set_int64(criteria, XPC_ACTIVITY_DELAY, 0);
    xpc_dictionary_set_int64(criteria, XPC_ACTIVITY_GRACE_PERIOD, 5);
    // we are downloading data and want to use WiFi instead of cellular
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY, true);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REQUIRE_INEXPENSIVE_NETWORK_CONNECTIVITY, true);
    xpc_dictionary_set_string(criteria, XPC_ACTIVITY_NETWORK_TRANSFER_DIRECTION, XPC_ACTIVITY_NETWORK_TRANSFER_DIRECTION_DOWNLOAD);

    if (request->criteria) {
        xpc_release(request->criteria);
    }
    request->criteria = criteria;

    xpc_activity_register("com.apple.trustd.validupdate", criteria, ^(xpc_activity_t activity) {
        xpc_activity_state_t activityState = xpc_activity_get_state(activity);
        switch (activityState) {
            case XPC_ACTIVITY_STATE_CHECK_IN: {
                secdebug("validupdate", "xpc activity state: XPC_ACTIVITY_STATE_CHECK_IN");
                break;
            }
            case XPC_ACTIVITY_STATE_RUN: {
                secdebug("validupdate", "xpc activity state: XPC_ACTIVITY_STATE_RUN");
                if (!xpc_activity_set_state(activity, XPC_ACTIVITY_STATE_CONTINUE)) {
                    secnotice("validupdate", "unable to set activity state to XPC_ACTIVITY_STATE_CONTINUE");
                }
                // criteria for this activity have been met; issue the network request
                SecValidUpdateRequestIssue(request);
                gUpdateRequestScheduled = 0.0;
                if (!xpc_activity_set_state(activity, XPC_ACTIVITY_STATE_DONE)) {
                    secnotice("validupdate", "unable to set activity state to XPC_ACTIVITY_STATE_DONE");
                }
                break;
            }
            default: {
                secdebug("validupdate", "unhandled activity state (%ld)", (long)activityState);
                break;
            }
        }
    });

    return true;
}

static bool SecValidUpdateRequestConsumeReply(CF_CONSUMED CFDataRef data, CFIndex version, bool save) {
    if (!data) {
        secnotice("validupdate", "invalid data");
        return false;
    }
    CFIndex length = CFDataGetLength(data);
    secdebug("validupdate", "data received: %ld bytes", (long)length);

    char *curPathBuf = NULL;
    if (save) {
        checkBasePath(kSecRevocationBasePath);
        asprintf(&curPathBuf, "%s/%s.plist.gz", kSecRevocationBasePath, "update-current");
    }
    // expand compressed data
    CFDataRef inflatedData = copyInflatedData(data);
    if (inflatedData) {
        CFIndex cmplength = length;
        length = CFDataGetLength(inflatedData);
        if (curPathBuf) {
            uint32_t crc = calculateCrc32(inflatedData);
            writeFile(curPathBuf, CFDataGetBytePtr(data), cmplength, crc, (uint32_t)length);
        }
        CFReleaseSafe(data);
        data = inflatedData;
    }
    secdebug("validupdate", "data expanded: %ld bytes", (long)length);

    // mmap the expanded data while property list object is created
    CFPropertyListRef propertyList = NULL;
    char *expPathBuf = NULL;
    asprintf(&expPathBuf, "%s/%s.plist", kSecRevocationBasePath, "update-current");
    if (expPathBuf) {
        writeFile(expPathBuf, CFDataGetBytePtr(data), length, 0, (uint32_t)length);
        CFReleaseNull(data);
        // no copies of data should exist in memory at this point
        int fd = open(expPathBuf, O_RDONLY);
        if (fd < 0) {
            secerror("unable to open %s (errno %d)", expPathBuf, errno);
        }
        else {
            void *p = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
            if (!p || p == MAP_FAILED) {
                secerror("unable to map %s (errno %d)", expPathBuf, errno);
            }
            else {
                data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)p, length, kCFAllocatorNull);
                if (data) {
                    propertyList = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                                                kCFPropertyListImmutable, NULL, NULL);
                }
                int rtn = munmap(p, length);
                if (rtn != 0) {
                    secerror("unable to unmap %ld bytes at %p (error %d)", (long)length, p, rtn);
                }
            }
            (void)close(fd);
        }
        // all done with this file
        (void)remove(expPathBuf);
        free(expPathBuf);
    }
    CFReleaseSafe(data);

    CFIndex curVersion = version;
    Boolean fullUpdate = false;
    if (isDictionary(propertyList)) {
        if (SecRevocationDbVerifyUpdate((CFDictionaryRef)propertyList)) {
            CFTypeRef value = (CFBooleanRef)CFDictionaryGetValue((CFDictionaryRef)propertyList, CFSTR("full"));
            if (isBoolean(value)) {
                fullUpdate = CFBooleanGetValue((CFBooleanRef)value);
            }
            curVersion = SecRevocationDbIngestUpdate((CFDictionaryRef)propertyList);
            gNextUpdate = SecRevocationDbComputeNextUpdateTime((CFDictionaryRef)propertyList);
        }
    } else {
        secerror("update failed: could not create property list");
    }
    CFReleaseSafe(propertyList);

    if (curVersion > version) {
        secdebug("validupdate", "update received: v%ld", (unsigned long)curVersion);
        // save this update and make it current
        char *newPathBuf = NULL;
        if (fullUpdate) {
            asprintf(&newPathBuf, "%s/update-full.plist.gz", kSecRevocationBasePath);
            //%%% glob and remove all "update-v*.plist.gz" files here
        }
        else {
            asprintf(&newPathBuf, "%s/update-v%ld.plist.gz", kSecRevocationBasePath, (unsigned long)curVersion);
        }
        if (newPathBuf) {
            if (curPathBuf) {
                if (fullUpdate) {
                    // try to save the latest full update
                    (void)rename(curPathBuf, newPathBuf);
                }
                else {
                    // try to remove delta updates
                    (void)remove(curPathBuf);
                }
            }
            free(newPathBuf);
        }
        gLastVersion = curVersion;
    }
    if (curPathBuf) {
        free(curPathBuf);
    }

    // remember next update time in case of restart
    SecRevocationDbSetNextUpdateTime(gNextUpdate);

    return true;
}

static bool SecValidUpdateRequestSatisfiedLocally(SecValidUpdateRequestRef request) {
    // if we can read the requested data locally, we don't need a network request.

    // note: only need this if we don't have any version and are starting from scratch.
    // otherwise we don't know what the current version actually is; only the server
    // can tell us that at any given time, so we have to ask it for any version >0.
    // we cannot reuse a saved delta without being on the exact version from which
    // it was generated.
    // TBD:
    // - if requested version N is 0, and no 'update-full' in kSecRevocationBasePath,
    // call a OTATrustUtilities SPI to obtain static 'update-full' asset data.

    CFDataRef data = NULL;
    char *curPathBuf = NULL;
    if (0 == request->version) {
        asprintf(&curPathBuf, "%s/update-full.plist.gz", kSecRevocationBasePath);
    }
    else {
        return false;
        //asprintf(&curPathBuf, "%s/update-v%ld.plist.gz", kSecRevocationBasePath, (unsigned long)request->version);
    }
    if (curPathBuf) {
        secdebug("validupdate", "will read data from \"%s\"", curPathBuf);
        int rtn = readFile(curPathBuf, &data);
        free(curPathBuf);
        if (rtn) { CFReleaseNull(data); }
    }
    if (data) {
        secdebug("validupdate", "read %ld bytes from file", (long)CFDataGetLength(data));
        //%%% TBD dispatch this work on the request's queue and return true immediately
        return SecValidUpdateRequestConsumeReply(data, request->version, false);
    }
    return false;
}

static void SecValidUpdateRequestCompleted(asynchttp_t *http, CFTimeInterval maxAge) {
    // cast depends on http being first field in struct SecValidUpdateRequest.
    SecValidUpdateRequestRef request = (SecValidUpdateRequestRef)http;
    if (!request) {
        secnotice("validupdate", "no request to complete!");
        return;
    }
    CFDataRef data = (request->http.response) ? CFHTTPMessageCopyBody(request->http.response) : NULL;
    CFIndex version = request->version;
    SecValidUpdateRequestRelease(request);
    if (!data) {
        secdebug("validupdate", "no data received");
        return;
    }
    SecValidUpdateRequestConsumeReply(data, version, true);
}


// MARK: -
// MARK: SecValidInfoRef

/* ======================================================================
   SecValidInfoRef
   ======================================================================
 */

static SecValidInfoRef SecValidInfoCreate(SecValidInfoFormat format,
                                          CFOptionFlags flags,
                                          CFDataRef certHash,
                                          CFDataRef issuerHash) {
    SecValidInfoRef validInfo;
    validInfo = (SecValidInfoRef)calloc(1, sizeof(struct __SecValidInfo));
    if (!validInfo) { return NULL; }

    CFRetainSafe(certHash);
    CFRetainSafe(issuerHash);
    validInfo->format = format;
    validInfo->certHash = certHash;
    validInfo->issuerHash = issuerHash;
    validInfo->valid = (flags & kSecValidInfoAllowlist);
    validInfo->complete = (flags & kSecValidInfoComplete);
    validInfo->checkOCSP = (flags & kSecValidInfoCheckOCSP);
    validInfo->knownOnly = (flags & kSecValidInfoKnownOnly);
    validInfo->requireCT = (flags & kSecValidInfoRequireCT);

    return validInfo;
}

void SecValidInfoRelease(SecValidInfoRef validInfo) {
    if (validInfo) {
        CFReleaseSafe(validInfo->certHash);
        CFReleaseSafe(validInfo->issuerHash);
        free(validInfo);
    }
}


// MARK: -
// MARK: SecRevocationDb

/* ======================================================================
   SecRevocationDb
   ======================================================================
*/

/* SecRevocationDbCheckNextUpdate returns true if we dispatched an
   update request, otherwise false.
*/
bool SecRevocationDbCheckNextUpdate(void) {
    // are we the db owner instance?
    if (!isDbOwner()) {
        return false;
    }
    CFTypeRef value = NULL;

    // is it time to check?
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime minNextUpdate = now + gUpdateInterval;
    if (0 == gNextUpdate) {
        // first time we're called, check if we have a saved nextUpdate value
        gNextUpdate = SecRevocationDbGetNextUpdateTime();
        // pin to minimum first-time interval, so we don't perturb startup
        minNextUpdate = now + kSecMinUpdateInterval;
        if (gNextUpdate < minNextUpdate) {
            gNextUpdate = minNextUpdate;
        }
        // allow pref to override update interval, if it exists
        CFIndex interval = -1;
        value = (CFNumberRef)CFPreferencesCopyValue(kUpdateIntervalKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (isNumber(value)) {
            if (CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval)) {
                if (interval < kSecMinUpdateInterval) {
                    interval = kSecMinUpdateInterval;
                } else if (interval > kSecMaxUpdateInterval) {
                    interval = kSecMaxUpdateInterval;
                }
            }
        }
        CFReleaseNull(value);
        gUpdateInterval = kSecStdUpdateInterval;
        if (interval > 0) {
            gUpdateInterval = interval;
        }
    }
    if (gNextUpdate > now) {
        return false;
    }
    // set minimum next update time here in case we can't get an update
    gNextUpdate = minNextUpdate;

    // determine which server to query
    CFStringRef server;
    value = (CFStringRef)CFPreferencesCopyValue(kUpdateServerKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isString(value)) {
        server = (CFStringRef) CFRetain(value);
    } else {
        server = (CFStringRef) CFRetain(kValidUpdateServer);
    }
    CFReleaseNull(value);

    // determine what version we currently have
    CFIndex version = SecRevocationDbGetVersion();
    secdebug("validupdate", "got version %ld from db", (long)version);
    if (version <= 0) {
        if (gLastVersion > 0) {
            secdebug("validupdate", "error getting version; using last good version: %ld", (long)gLastVersion);
        }
        version = gLastVersion;
    }

    // determine whether we need to recreate the database
    CFIndex db_version = SecRevocationDbGetSchemaVersion();
    if (db_version == 1) {
        /* code which created this db failed to update changed flags,
           so we need to fully rebuild its contents. */
        SecRevocationDbRemoveAllEntries();
        version = gLastVersion = 0;
    }

    // determine whether update fetching is enabled
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 101300 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
    bool updateEnabled = true; // macOS 10.13 or iOS 11.0, not tvOS, not watchOS
#else
    bool updateEnabled = false;
#endif
    value = (CFBooleanRef)CFPreferencesCopyValue(kUpdateEnabledKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateEnabled = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);

    // set up a network request
    SecValidUpdateRequestRef request = (SecValidUpdateRequestRef)calloc(1, sizeof(*request));
    request->http.queue = SecRevocationDbGetUpdateQueue();
    request->http.completed = SecValidUpdateRequestCompleted;
    request->server = server;
    request->version = version;
    request->criteria = NULL;

    if (SecValidUpdateRequestSatisfiedLocally(request)) {
        SecValidUpdateRequestRelease(request);
        return true;
    }
    if (!updateEnabled) {
        SecValidUpdateRequestRelease(request);
        return false;
    }
    return SecValidUpdateRequestSchedule(request);
}

bool SecRevocationDbVerifyUpdate(CFDictionaryRef update) {

    //%%% TBD: check signature with new SecPolicyRef; rdar://28619456
    return true;
}

CFAbsoluteTime SecRevocationDbComputeNextUpdateTime(CFDictionaryRef update) {
    CFIndex interval = 0;
    // get server-provided interval
    if (update) {
        CFTypeRef value = (CFNumberRef)CFDictionaryGetValue(update, CFSTR("check-again"));
        if (isNumber(value)) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval);
        }
    }
    // try to use interval preference if it exists
    CFTypeRef value = (CFNumberRef)CFPreferencesCopyValue(kUpdateIntervalKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isNumber(value)) {
        CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval);
    }
    CFReleaseNull(value);

    // sanity check
    if (interval < kSecMinUpdateInterval) {
        interval = kSecMinUpdateInterval;
    } else if (interval > kSecMaxUpdateInterval) {
        interval = kSecMaxUpdateInterval;
    }

    // compute randomization factor, between 0 and 50% of the interval
    CFIndex fuzz = arc4random() % (long)(interval/2.0);
    CFAbsoluteTime nextUpdate =  CFAbsoluteTimeGetCurrent() + interval + fuzz;
    secdebug("validupdate", "next update in %ld seconds", (long)(interval + fuzz));
    return nextUpdate;
}

CFIndex SecRevocationDbIngestUpdate(CFDictionaryRef update) {
    CFIndex version = 0;
    if (!update) {
        return version;
    }
    CFTypeRef value = (CFNumberRef)CFDictionaryGetValue(update, CFSTR("version"));
    if (isNumber(value)) {
        if (!CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &version)) {
            version = 0;
        }
    }
    SecRevocationDbApplyUpdate(update, version);

    return version;
}

/* Database management */

/* v1 = initial version */
/* v2 = fix for group entry transitions */

#define kSecRevocationDbSchemaVersion   2

#define selectGroupIdSQL  CFSTR("SELECT DISTINCT groupid " \
"FROM issuers WHERE issuer_hash=?")

static SecDbRef SecRevocationDbCreate(CFStringRef path) {
    /* only the db owner should open a read-write connection. */
    bool readWrite = isDbOwner();
    mode_t mode = 0644;

    SecDbRef result = SecDbCreateWithOptions(path, mode, readWrite, false, false, ^bool (SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error) {
        __block bool ok = true;
        CFErrorRef localError = NULL;
        if (ok && !SecDbWithSQL(dbconn, selectGroupIdSQL, &localError, NULL) && CFErrorGetCode(localError) == SQLITE_ERROR) {
            /* SecDbWithSQL returns SQLITE_ERROR if the table we are preparing the above statement for doesn't exist. */

            /* admin table holds these key-value (or key-ival) pairs:
             'version' (integer)    // version of database content
             'check_again' (double) // CFAbsoluteTime of next check (optional; this value is currently stored in prefs)
             'db_version' (integer) // version of database schema
             'db_hash' (blob)       // SHA-256 database hash
             --> entries in admin table are unique by text key

             issuers table holds map of issuing CA hashes to group identifiers:
             issuer_hash (blob)    // SHA-256 hash of issuer certificate (primary key)
             groupid (integer)     // associated group identifier in group ID table
             --> entries in issuers table are unique by issuer_hash;
             multiple issuer entries may have the same groupid!

             groups table holds records with these attributes:
             groupid (integer)     // ordinal ID associated with this group entry
             flags (integer)       // a bitmask of the following values:
                kSecValidInfoComplete   (0x00000001) set if we have all revocation info for this issuer group
                kSecValidInfoCheckOCSP  (0x00000002) set if must check ocsp for certs from this issuer group
                kSecValidInfoKnownOnly  (0x00000004) set if any CA from this issuer group must be in database
                kSecValidInfoRequireCT  (0x00000008) set if all certs from this issuer group must have SCTs
                kSecValidInfoAllowlist  (0x00000010) set if this entry describes valid certs (i.e. is allowed)
             format (integer)      // an integer describing format of entries:
                kSecValidInfoFormatUnknown (0) unknown format
                kSecValidInfoFormatSerial  (1) serial number, not greater than 20 bytes in length
                kSecValidInfoFormatSHA256  (2) SHA-256 hash, 32 bytes in length
                kSecValidInfoFormatNto1    (3) filter data blob of arbitrary length
             data (blob)           // Bloom filter data if format is 'nto1', otherwise NULL
             --> entries in groups table are unique by groupid

             serials table holds serial number blobs with these attributes:
             rowid (integer)       // ordinal ID associated with this serial number entry
             serial (blob)         // serial number
             groupid (integer)     // identifier for issuer group in the groups table
             --> entries in serials table are unique by serial and groupid

             hashes table holds SHA-256 hashes of certificates with these attributes:
             rowid (integer)       // ordinal ID associated with this sha256 hash entry
             sha256 (blob)         // SHA-256 hash of subject certificate
             groupid (integer)     // identifier for issuer group in the groups table
             --> entries in hashes table are unique by sha256 and groupid
             */
            ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
                ok = SecDbExec(dbconn,
                               CFSTR("CREATE TABLE admin("
                                     "key TEXT PRIMARY KEY NOT NULL,"
                                     "ival INTEGER NOT NULL,"
                                     "value BLOB"
                                     ");"
                                     "CREATE TABLE issuers("
                                     "issuer_hash BLOB PRIMARY KEY NOT NULL,"
                                     "groupid INTEGER NOT NULL"
                                     ");"
                                     "CREATE INDEX issuer_idx ON issuers(issuer_hash);"
                                     "CREATE TABLE groups("
                                     "groupid INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "flags INTEGER NOT NULL,"
                                     "format INTEGER NOT NULL,"
                                     "data BLOB"
                                     ");"
                                     "CREATE TABLE serials("
                                     "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "serial BLOB NOT NULL,"
                                     "groupid INTEGER NOT NULL,"
                                     "UNIQUE(serial,groupid)"
                                     ");"
                                     "CREATE TABLE hashes("
                                     "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "sha256 BLOB NOT NULL,"
                                     "groupid INTEGER NOT NULL,"
                                     "UNIQUE(sha256,groupid)"
                                     ");"
                                     "CREATE TRIGGER group_del BEFORE DELETE ON groups FOR EACH ROW "
                                     "BEGIN "
                                     "DELETE FROM serials WHERE groupid=OLD.groupid; "
                                     "DELETE FROM hashes WHERE groupid=OLD.groupid; "
                                     "DELETE FROM issuers WHERE groupid=OLD.groupid; "
                                     "END;"), error);
                *commit = ok;
            });
        }
        CFReleaseSafe(localError);
        if (!ok)
            secerror("%s failed: %@", didCreate ? "Create" : "Open", error ? *error : NULL);
        return ok;
    });

    return result;
}

typedef struct __SecRevocationDb *SecRevocationDbRef;
struct __SecRevocationDb {
    SecDbRef db;
    dispatch_queue_t update_queue;
    bool fullUpdateInProgress;
};

static dispatch_once_t kSecRevocationDbOnce;
static SecRevocationDbRef kSecRevocationDb = NULL;

static SecRevocationDbRef SecRevocationDbInit(CFStringRef db_name) {
    SecRevocationDbRef this;
    dispatch_queue_attr_t attr;

    require(this = (SecRevocationDbRef)malloc(sizeof(struct __SecRevocationDb)), errOut);
    this->db = NULL;
    this->update_queue = NULL;
    this->fullUpdateInProgress = false;

    require(this->db = SecRevocationDbCreate(db_name), errOut);
    attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_BACKGROUND, 0);
    require(this->update_queue = dispatch_queue_create(NULL, attr), errOut);

    return this;

errOut:
    secdebug("validupdate", "Failed to create db at \"%@\"", db_name);
    if (this) {
        if (this->update_queue) {
            dispatch_release(this->update_queue);
        }
        CFReleaseSafe(this->db);
        free(this);
    }
    return NULL;
}

static CFStringRef SecRevocationDbCopyPath(void) {
    CFURLRef revDbURL = NULL;
    CFStringRef revInfoRelPath = NULL;
    if ((revInfoRelPath = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), kSecRevocationDbFileName)) != NULL) {
        revDbURL = SecCopyURLForFileInRevocationInfoDirectory(revInfoRelPath);
    }
    CFReleaseSafe(revInfoRelPath);

    CFStringRef revDbPath = NULL;
    if (revDbURL) {
        revDbPath = CFURLCopyFileSystemPath(revDbURL, kCFURLPOSIXPathStyle);
        CFRelease(revDbURL);
    }
    return revDbPath;
}

static void SecRevocationDbWith(void(^dbJob)(SecRevocationDbRef db)) {
    dispatch_once(&kSecRevocationDbOnce, ^{
        CFStringRef dbPath = SecRevocationDbCopyPath();
        if (dbPath) {
            kSecRevocationDb = SecRevocationDbInit(dbPath);
            CFRelease(dbPath);
        }
    });
    // Do pre job run work here (cancel idle timers etc.)
    if (kSecRevocationDb->fullUpdateInProgress) {
        return; // this would block since SecDb has an exclusive transaction lock
    }
    dbJob(kSecRevocationDb);
    // Do post job run work here (gc timer, etc.)
}

/* Instance implementation. */

#define selectVersionSQL CFSTR("SELECT ival FROM admin " \
"WHERE key='version'")
#define selectDbVersionSQL CFSTR("SELECT ival FROM admin " \
"WHERE key='db_version'")
#define selectDbHashSQL CFSTR("SELECT value FROM admin " \
"WHERE key='db_hash'")
#define selectNextUpdateSQL CFSTR("SELECT value FROM admin " \
"WHERE key='check_again'")
#define selectGroupRecordSQL CFSTR("SELECT flags,format,data FROM " \
"groups WHERE groupid=?")
#define selectSerialRecordSQL CFSTR("SELECT rowid FROM serials " \
"WHERE serial=? AND groupid=?")
#define selectHashRecordSQL CFSTR("SELECT rowid FROM hashes " \
"WHERE sha256=? AND groupid=?")
#define insertAdminRecordSQL CFSTR("INSERT OR REPLACE INTO admin " \
"(key,ival,value) VALUES (?,?,?)")
#define insertIssuerRecordSQL CFSTR("INSERT OR REPLACE INTO issuers " \
"(issuer_hash,groupid) VALUES (?,?)")
#define insertGroupRecordSQL CFSTR("INSERT OR REPLACE INTO groups " \
"(groupid,flags,format,data) VALUES (?,?,?,?)")
#define insertSerialRecordSQL CFSTR("INSERT OR REPLACE INTO serials " \
"(rowid,serial,groupid) VALUES (?,?,?)")
#define insertSha256RecordSQL CFSTR("INSERT OR REPLACE INTO hashes " \
"(rowid,sha256,groupid) VALUES (?,?,?)")
#define deleteGroupRecordSQL CFSTR("DELETE FROM groups WHERE groupid=?")

#define deleteAllEntriesSQL CFSTR("DELETE from hashes; " \
"DELETE from serials; DELETE from issuers; DELETE from groups; " \
"DELETE from admin; DELETE from sqlite_sequence; VACUUM")

static int64_t _SecRevocationDbGetVersion(SecRevocationDbRef this, CFErrorRef *error) {
    /* look up version entry in admin table; returns -1 on error */
    __block int64_t version = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        if (ok) ok &= SecDbWithSQL(dbconn, selectVersionSQL, &localError, ^bool(sqlite3_stmt *selectVersion) {
            ok = SecDbStep(dbconn, selectVersion, &localError, NULL);
            version = sqlite3_column_int64(selectVersion, 0);
            return ok;
        });
    });
    (void) CFErrorPropagate(localError, error);
    return version;
}

static void _SecRevocationDbSetVersion(SecRevocationDbRef this, CFIndex version){
    secdebug("validupdate", "setting version to %ld", (long)version);

    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            if (ok) ok = SecDbWithSQL(dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertVersion) {
                if (ok) {
                    const char *versionKey = "version";
                    ok = SecDbBindText(insertVersion, 1, versionKey, strlen(versionKey),
                                       SQLITE_TRANSIENT, &localError);
                }
                if (ok) {
                    ok = SecDbBindInt64(insertVersion, 2,
                                        (sqlite3_int64)version, &localError);
                }
                if (ok) {
                    ok = SecDbStep(dbconn, insertVersion, &localError, NULL);
                }
                return ok;
            });
        });
    });
    if (!ok) {
        secerror("_SecRevocationDbSetVersion failed: %@", localError);
    }
    CFReleaseSafe(localError);
}

static int64_t _SecRevocationDbGetSchemaVersion(SecRevocationDbRef this, CFErrorRef *error) {
    /* look up db_version entry in admin table; returns -1 on error */
    __block int64_t db_version = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        if (ok) ok &= SecDbWithSQL(dbconn, selectDbVersionSQL, &localError, ^bool(sqlite3_stmt *selectDbVersion) {
            ok = SecDbStep(dbconn, selectDbVersion, &localError, NULL);
            db_version = sqlite3_column_int64(selectDbVersion, 0);
            return ok;
        });
    });
    (void) CFErrorPropagate(localError, error);
    return db_version;
}

static void _SecRevocationDbSetSchemaVersion(SecRevocationDbRef this, CFIndex dbversion){
    secdebug("validupdate", "setting db_version to %ld", (long)dbversion);

    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            if (ok) ok = SecDbWithSQL(dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertDbVersion) {
                if (ok) {
                    const char *dbVersionKey = "db_version";
                    ok = SecDbBindText(insertDbVersion, 1, dbVersionKey, strlen(dbVersionKey),
                                       SQLITE_TRANSIENT, &localError);
                }
                if (ok) {
                    ok = SecDbBindInt64(insertDbVersion, 2,
                                        (sqlite3_int64)dbversion, &localError);
                }
                if (ok) {
                    ok = SecDbStep(dbconn, insertDbVersion, &localError, NULL);
                }
                return ok;
            });
        });
    });
    if (!ok) {
        secerror("_SecRevocationDbSetSchemaVersion failed: %@", localError);
    }
    CFReleaseSafe(localError);
}

static CFAbsoluteTime _SecRevocationDbGetNextUpdateTime(SecRevocationDbRef this, CFErrorRef *error) {
    /* look up check_again entry in admin table; returns 0 on error */
    __block CFAbsoluteTime nextUpdate = 0;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        if (ok) ok &= SecDbWithSQL(dbconn, selectNextUpdateSQL, &localError, ^bool(sqlite3_stmt *selectNextUpdate) {
            ok = SecDbStep(dbconn, selectNextUpdate, &localError, NULL);
            CFAbsoluteTime *p = (CFAbsoluteTime *)sqlite3_column_blob(selectNextUpdate, 0);
            if (p != NULL) {
                if (sizeof(CFAbsoluteTime) == sqlite3_column_bytes(selectNextUpdate, 0)) {
                    nextUpdate = *p;
                }
            }
            return ok;
        });
    });

    (void) CFErrorPropagate(localError, error);
    return nextUpdate;
}

static void _SecRevocationDbSetNextUpdateTime(SecRevocationDbRef this, CFAbsoluteTime nextUpdate){
    secdebug("validupdate", "setting next update to %f", (double)nextUpdate);

    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            if (ok) ok = SecDbWithSQL(dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertRecord) {
                if (ok) {
                    const char *nextUpdateKey = "check_again";
                    ok = SecDbBindText(insertRecord, 1, nextUpdateKey, strlen(nextUpdateKey),
                                       SQLITE_TRANSIENT, &localError);
                }
                if (ok) {
                    ok = SecDbBindInt64(insertRecord, 2,
                                        (sqlite3_int64)0, &localError);
                }
                if (ok) {
                    ok = SecDbBindBlob(insertRecord, 3,
                                       &nextUpdate, sizeof(CFAbsoluteTime),
                                       SQLITE_TRANSIENT, &localError);
                }
                if (ok) {
                    ok = SecDbStep(dbconn, insertRecord, &localError, NULL);
                }
                return ok;
            });
        });
    });
    if (!ok) {
        secerror("_SecRevocationDbSetNextUpdate failed: %@", localError);
    }
    CFReleaseSafe(localError);
}

static bool _SecRevocationDbRemoveAllEntries(SecRevocationDbRef this) {
    /* remove all entries from all tables in the database */
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            ok &= SecDbWithSQL(dbconn, deleteAllEntriesSQL, &localError, ^bool(sqlite3_stmt *deleteAll) {
                ok = SecDbStep(dbconn, deleteAll, &localError, NULL);
                return ok;
            });
        });
    });
    /* one more thing: update the schema version */
    _SecRevocationDbSetSchemaVersion(this, kSecRevocationDbSchemaVersion);

    CFReleaseSafe(localError);
    return ok;
}

static bool _SecRevocationDbUpdateIssuers(SecRevocationDbRef this, int64_t groupId, CFArrayRef issuers, CFErrorRef *error) {
    /* insert or replace issuer records in issuers table */
    if (!issuers || groupId < 0) {
        return false; /* must have something to insert, and a group to associate with it */
    }
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            if (isArray(issuers)) {
                CFIndex issuerIX, issuerCount = CFArrayGetCount(issuers);
                for (issuerIX=0; issuerIX<issuerCount && ok; issuerIX++) {
                    CFDataRef hash = (CFDataRef)CFArrayGetValueAtIndex(issuers, issuerIX);
                    if (!hash) { continue; }
                    if (ok) ok = SecDbWithSQL(dbconn, insertIssuerRecordSQL, &localError, ^bool(sqlite3_stmt *insertIssuer) {
                        if (ok) {
                            ok = SecDbBindBlob(insertIssuer, 1,
                                               CFDataGetBytePtr(hash),
                                               CFDataGetLength(hash),
                                               SQLITE_TRANSIENT, &localError);
                        }
                        if (ok) {
                            ok = SecDbBindInt64(insertIssuer, 2,
                                                groupId, &localError);
                        }
                        /* Execute the insert statement for this issuer record. */
                        if (ok) {
                            ok = SecDbStep(dbconn, insertIssuer, &localError, NULL);
                        }
                        return ok;
                    });
                }
            }
        });
    });

    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbUpdatePerIssuerData(SecRevocationDbRef this, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* insert records in serials or hashes table. */
    if (!dict || groupId < 0) {
        return false; /* must have something to insert, and a group to associate with it */
    }
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            CFArrayRef addArray = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("add"));
            if (isArray(addArray)) {
                CFIndex identifierIX, identifierCount = CFArrayGetCount(addArray);
                for (identifierIX=0; identifierIX<identifierCount; identifierIX++) {
                    CFDataRef identifierData = (CFDataRef)CFArrayGetValueAtIndex(addArray, identifierIX);
                    if (!identifierData) { continue; }
                    CFIndex length = CFDataGetLength(identifierData);
                    /* we can figure out the format without an extra read to get the format column.
                       len <= 20 is a serial number. len==32 is a sha256 hash. otherwise: xor. */
                    CFStringRef sql = NULL;
                    if (length <= 20) {
                        sql = insertSerialRecordSQL;
                    } else if (length == 32) {
                        sql = insertSha256RecordSQL;
                    }
                    if (!sql) { continue; }

                    if (ok) ok = SecDbWithSQL(dbconn, sql, &localError, ^bool(sqlite3_stmt *insertIdentifier) {
                        /* (rowid,serial|sha256,groupid) */
                        /* rowid == column 1, autoincrement so we don't set directly */
                        if (ok) {
                            ok = SecDbBindBlob(insertIdentifier, 2,
                                               CFDataGetBytePtr(identifierData),
                                               CFDataGetLength(identifierData),
                                               SQLITE_TRANSIENT, &localError);
                        }
                        if (ok) {
                            ok = SecDbBindInt64(insertIdentifier, 3,
                                                groupId, &localError);
                        }
                        /* Execute the insert statement for the identifier record. */
                        if (ok) {
                            ok = SecDbStep(dbconn, insertIdentifier, &localError, NULL);
                        }
                        return ok;
                    });
                }
            }
        });
    });

    (void) CFErrorPropagate(localError, error);
    return ok;
}

static int64_t _SecRevocationDbUpdateGroup(SecRevocationDbRef this, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* insert group record for a given groupId.
       if the specified groupId is < 0, a new group entry is created.
       returns the groupId on success, or -1 on failure.
     */
    __block int64_t result = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    if (!dict) {
        return groupId; /* no-op if no dictionary is provided */
    }

    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            ok &= SecDbWithSQL(dbconn, insertGroupRecordSQL, &localError, ^bool(sqlite3_stmt *insertGroup) {
                CFTypeRef value;
                SecValidInfoFormat format = kSecValidInfoFormatUnknown;
                /* (groupid,flags,format,data) */
                /* groups.groupid */
                if (ok && !(groupId < 0)) {
                    /* bind to existing groupId row if known, otherwise will insert and autoincrement */
                    ok = SecDbBindInt64(insertGroup, 1, groupId, &localError);
                }
                /* groups.flags */
                if (ok) {
                    int flags = 0;
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("complete"));
                    if (isBoolean(value)) {
                        if (CFBooleanGetValue((CFBooleanRef)value)) {
                            flags |= kSecValidInfoComplete;
                        }
                    }
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("check-ocsp"));
                    if (isBoolean(value)) {
                        if (CFBooleanGetValue((CFBooleanRef)value)) {
                            flags |= kSecValidInfoCheckOCSP;
                        }
                    }
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("known-intermediates-only"));
                    if (isBoolean(value)) {
                        if (CFBooleanGetValue((CFBooleanRef)value)) {
                            flags |= kSecValidInfoKnownOnly;
                        }
                    }
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("require-ct"));
                    if (isBoolean(value)) {
                        if (CFBooleanGetValue((CFBooleanRef)value)) {
                            flags |= kSecValidInfoRequireCT;
                        }
                    }
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("valid"));
                    if (isBoolean(value)) {
                        if (CFBooleanGetValue((CFBooleanRef)value)) {
                            flags |= kSecValidInfoAllowlist;
                        }
                    }
                    ok = SecDbBindInt(insertGroup, 2, flags, &localError);
                }
                /* groups.format */
                if (ok) {
                    value = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("format"));
                    if (isString(value)) {
                        if (CFStringCompare((CFStringRef)value, CFSTR("serial"), 0) == kCFCompareEqualTo) {
                            format = kSecValidInfoFormatSerial;
                        } else if (CFStringCompare((CFStringRef)value, CFSTR("sha256"), 0) == kCFCompareEqualTo) {
                            format = kSecValidInfoFormatSHA256;
                        } else if (CFStringCompare((CFStringRef)value, CFSTR("nto1"), 0) == kCFCompareEqualTo) {
                            format = kSecValidInfoFormatNto1;
                        }
                    }
                    ok = SecDbBindInt(insertGroup, 3, (int)format, &localError);
                }
                /* groups.data */
                CFDataRef xmlData = NULL;
                if (ok && format == kSecValidInfoFormatNto1) {
                    CFMutableDictionaryRef nto1 = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                            &kCFTypeDictionaryKeyCallBacks,
                                                                            &kCFTypeDictionaryValueCallBacks);
                    value = (CFDataRef)CFDictionaryGetValue(dict, CFSTR("xor"));
                    if (isData(value)) {
                        CFDictionaryAddValue(nto1, CFSTR("xor"), value);
                    }
                    value = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("params"));
                    if (isArray(value)) {
                        CFDictionaryAddValue(nto1, CFSTR("params"), value);
                    }
                    xmlData = CFPropertyListCreateData(kCFAllocatorDefault, nto1,
                                                       kCFPropertyListXMLFormat_v1_0, 0, &localError);
                    CFReleaseSafe(nto1);

                    if (xmlData) {
                        // compress the xmlData blob, if possible
                        CFDataRef deflatedData = copyDeflatedData(xmlData);
                        if (deflatedData) {
                            if (CFDataGetLength(deflatedData) < CFDataGetLength(xmlData)) {
                                CFRelease(xmlData);
                                xmlData = deflatedData;
                            }
                            else {
                                CFRelease(deflatedData);
                            }
                        }
                        ok = SecDbBindBlob(insertGroup, 4,
                                           CFDataGetBytePtr(xmlData),
                                           CFDataGetLength(xmlData),
                                           SQLITE_TRANSIENT, &localError);
                    }
                }

                /* Execute the insert statement for the group record. */
                if (ok) {
                    ok = SecDbStep(dbconn, insertGroup, &localError, NULL);
                    result = (int64_t)sqlite3_last_insert_rowid(SecDbHandle(dbconn));
                }
                if (!ok) {
                    secdebug("validupdate", "Failed to insert group %ld", (long)result);
                }
                /* Clean up temporary allocation made in this block. */
                CFReleaseSafe(xmlData);
                return ok;
            });
        });
    });

    (void) CFErrorPropagate(localError, error);
    return result;
}

static int64_t _SecRevocationDbGroupIdForIssuerHash(SecRevocationDbRef this, CFDataRef hash, CFErrorRef *error) {
    /* look up issuer hash in issuers table to get groupid, if it exists */
    __block int64_t groupId = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    require(hash, errOut);

    /* This is the starting point for any lookup; find a group id for the given issuer hash.
       Before we do that, need to verify the current db_version. We cannot use results from a
       database created with schema version 1. At the next database update interval,
       we'll be removing and recreating the database contents with the current schema version.
    */
    int64_t db_version = _SecRevocationDbGetSchemaVersion(this, NULL);
    require(db_version > 1, errOut);

    /* Look up provided issuer_hash in the issuers table.
    */
    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectGroupIdSQL, &localError, ^bool(sqlite3_stmt *selectGroupId) {
            ok = SecDbBindBlob(selectGroupId, 1, CFDataGetBytePtr(hash), CFDataGetLength(hash), SQLITE_TRANSIENT, &localError);
            ok &= SecDbStep(dbconn, selectGroupId, &localError, ^(bool *stopGroupId) {
                groupId = sqlite3_column_int64(selectGroupId, 0);
            });
            return ok;
        });
    });

errOut:
    (void) CFErrorPropagate(localError, error);
    return groupId;
}

static bool _SecRevocationDbApplyGroupDelete(SecRevocationDbRef this, CFDataRef issuerHash, CFErrorRef *error) {
    /* delete group associated with the given issuer;
       schema trigger will delete associated issuers, serials, and hashes. */
    __block int64_t groupId = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    groupId = _SecRevocationDbGroupIdForIssuerHash(this, issuerHash, &localError);
    require(!(groupId < 0), errOut);

    ok &= SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            ok = SecDbWithSQL(dbconn, deleteGroupRecordSQL, &localError, ^bool(sqlite3_stmt *deleteResponse) {
                ok = SecDbBindInt64(deleteResponse, 1, groupId, &localError);
                /* Execute the delete statement. */
                if (ok) {
                    ok = SecDbStep(dbconn, deleteResponse, &localError, NULL);
                }
                return ok;
            });
        });
    });

errOut:
    (void) CFErrorPropagate(localError, error);
    return (groupId < 0) ? false : true;
}

static bool _SecRevocationDbApplyGroupUpdate(SecRevocationDbRef this, CFDictionaryRef dict, CFErrorRef *error) {
    /* process one issuer group's update dictionary */
    int64_t groupId = -1;
    CFErrorRef localError = NULL;

    CFArrayRef issuers = (dict) ? (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("issuer-hash")) : NULL;
    if (isArray(issuers)) {
        CFIndex issuerIX, issuerCount = CFArrayGetCount(issuers);
        /* while we have issuers and haven't found a matching group id */
        for (issuerIX=0; issuerIX<issuerCount && groupId < 0; issuerIX++) {
            CFDataRef hash = (CFDataRef)CFArrayGetValueAtIndex(issuers, issuerIX);
            if (!hash) { continue; }
            groupId = _SecRevocationDbGroupIdForIssuerHash(this, hash, &localError);
        }
    }
    /* create or update the group entry */
    groupId = _SecRevocationDbUpdateGroup(this, groupId, dict, &localError);
    if (groupId < 0) {
        secdebug("validupdate", "failed to get groupId");
    } else {
        //secdebug("validupdate", "got groupId %ld", (long)groupId);
        /* create or update issuer entries, now that we know the group id */
        _SecRevocationDbUpdateIssuers(this, groupId, issuers, &localError);
        /* create or update entries in serials or hashes tables */
        _SecRevocationDbUpdatePerIssuerData(this, groupId, dict, &localError);
    }

    (void) CFErrorPropagate(localError, error);
    return (groupId < 0) ? false : true;
}

static void _SecRevocationDbApplyUpdate(SecRevocationDbRef this, CFDictionaryRef update, CFIndex version) {
    /* process entire update dictionary */
    if (!this || !update) {
        secerror("_SecRevocationDbApplyUpdate failed: invalid args");
        return;
    }
    CFRetain(update);

    __block CFDictionaryRef localUpdate = update;
    __block CFErrorRef localError = NULL;

    // This may take a while; do the work on our update queue with background priority.

    dispatch_async(this->update_queue, ^{

    CFTypeRef value;
    CFIndex deleteCount = 0;
    CFIndex updateCount = 0;

    /* check whether this is a full update */
    this->fullUpdateInProgress = false;
    value = (CFBooleanRef)CFDictionaryGetValue(update, CFSTR("full"));
    if (isBoolean(value)) {
        this->fullUpdateInProgress = CFBooleanGetValue((CFBooleanRef)value);
    }

    /* process 'delete' list */
    value = (CFArrayRef)CFDictionaryGetValue(localUpdate, CFSTR("delete"));
    if (isArray(value)) {
        deleteCount = CFArrayGetCount((CFArrayRef)value);
        secdebug("validupdate", "processing %ld deletes", (long)deleteCount);
        for (CFIndex deleteIX=0; deleteIX<deleteCount; deleteIX++) {
            CFDataRef issuerHash = (CFDataRef)CFArrayGetValueAtIndex((CFArrayRef)value, deleteIX);
            if (isData(issuerHash)) {
                (void)_SecRevocationDbApplyGroupDelete(this, issuerHash, &localError);
                CFReleaseNull(localError);
            }
        }
    }

    /* process 'update' list */
    value = (CFArrayRef)CFDictionaryGetValue(localUpdate, CFSTR("update"));
    if (isArray(value)) {
        updateCount = CFArrayGetCount((CFArrayRef)value);
        secdebug("validupdate", "processing %ld updates", (long)updateCount);
        for (CFIndex updateIX=0; updateIX<updateCount; updateIX++) {
            CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex((CFArrayRef)value, updateIX);
            if (isDictionary(dict)) {
                (void)_SecRevocationDbApplyGroupUpdate(this, dict, &localError);
                CFReleaseNull(localError);
            }
        }
    }
    CFRelease(localUpdate);

    /* set version */
    _SecRevocationDbSetVersion(this, version);

    /* set db_version if not already set */
    int64_t db_version = _SecRevocationDbGetSchemaVersion(this, NULL);
    if (db_version < 0) {
        _SecRevocationDbSetSchemaVersion(this, kSecRevocationDbSchemaVersion);
    }

    /* compact the db */
    (void)SecDbPerformWrite(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
            SecDbExec(dbconn, CFSTR("VACUUM;"), &localError);
            CFReleaseNull(localError);
        });
    });
    this->fullUpdateInProgress = false;

    });
}

static bool _SecRevocationDbSerialInGroup(SecRevocationDbRef this,
                                          CFDataRef serial,
                                          int64_t groupId,
                                          CFErrorRef *error) {
    __block bool result = false;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    require(this && serial, errOut);
    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectSerialRecordSQL, &localError, ^bool(sqlite3_stmt *selectSerial) {
            ok &= SecDbBindBlob(selectSerial, 1, CFDataGetBytePtr(serial),
                                CFDataGetLength(serial), SQLITE_TRANSIENT, &localError);
            ok &= SecDbBindInt64(selectSerial, 2, groupId, &localError);
            ok &= SecDbStep(dbconn, selectSerial, &localError, ^(bool *stop) {
                int64_t foundRowId = (int64_t)sqlite3_column_int64(selectSerial, 0);
                result = (foundRowId > 0);
            });
            return ok;
        });
    });

errOut:
    (void) CFErrorPropagate(localError, error);
    return result;
}

static bool _SecRevocationDbCertHashInGroup(SecRevocationDbRef this,
                                            CFDataRef certHash,
                                            int64_t groupId,
                                            CFErrorRef *error) {
    __block bool result = false;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    require(this && certHash, errOut);
    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectHashRecordSQL, &localError, ^bool(sqlite3_stmt *selectHash) {
            ok = SecDbBindBlob(selectHash, 1, CFDataGetBytePtr(certHash),
                               CFDataGetLength(certHash), SQLITE_TRANSIENT, &localError);
            ok &= SecDbBindInt64(selectHash, 2, groupId, &localError);
            ok &= SecDbStep(dbconn, selectHash, &localError, ^(bool *stop) {
                int64_t foundRowId = (int64_t)sqlite3_column_int64(selectHash, 0);
                result = (foundRowId > 0);
            });
            return ok;
        });
    });

errOut:
    (void) CFErrorPropagate(localError, error);
    return result;
}

static bool _SecRevocationDbSerialInFilter(SecRevocationDbRef this,
                                           CFDataRef serialData,
                                           CFDataRef xmlData) {
    /* N-To-1 filter implementation.
       The 'xmlData' parameter is a flattened XML dictionary,
       containing 'xor' and 'params' keys. First order of
       business is to reconstitute the blob into components.
    */
    bool result = false;
    CFRetainSafe(xmlData);
    CFDataRef propListData = xmlData;
    /* Expand data blob if needed */
    CFDataRef inflatedData = copyInflatedData(propListData);
    if (inflatedData) {
        CFReleaseSafe(propListData);
        propListData = inflatedData;
    }
    CFDataRef xor = NULL;
    CFArrayRef params = NULL;
    CFPropertyListRef nto1 = CFPropertyListCreateWithData(kCFAllocatorDefault, propListData, 0, NULL, NULL);
    if (nto1) {
        xor = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("xor"));
        params = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("params"));
    }
    uint8_t *hash = (xor) ? (uint8_t*)CFDataGetBytePtr(xor) : NULL;
    CFIndex hashLen = (hash) ? CFDataGetLength(xor) : 0;
    uint8_t *serial = (serialData) ? (uint8_t*)CFDataGetBytePtr(serialData) : NULL;
    CFIndex serialLen = (serial) ? CFDataGetLength(serialData) : 0;

    require(hash && serial && params, errOut);

    const uint32_t FNV_OFFSET_BASIS = 2166136261;
    const uint32_t FNV_PRIME = 16777619;
    bool notInHash = false;
    CFIndex ix, count = CFArrayGetCount(params);
    for (ix = 0; ix < count; ix++) {
        int32_t param;
        CFNumberRef cfnum = (CFNumberRef)CFArrayGetValueAtIndex(params, ix);
        if (!isNumber(cfnum) ||
            !CFNumberGetValue(cfnum, kCFNumberSInt32Type, &param)) {
            secinfo("validupdate", "error processing filter params at index %ld", (long)ix);
            continue;
        }
        /* process one param */
        uint32_t hval = FNV_OFFSET_BASIS ^ param;
        CFIndex i = serialLen;
        while (i > 0) {
            hval = ((hval ^ (serial[--i])) * FNV_PRIME) & 0xFFFFFFFF;
        }
        hval = hval % (hashLen * 8);
        if ((hash[hval/8] & (1 << (hval % 8))) == 0) {
            notInHash = true; /* definitely not in hash */
            break;
        }
    }
    if (!notInHash) {
        /* probabilistically might be in hash if we get here. */
        result = true;
    }

errOut:
    CFReleaseSafe(nto1);
    CFReleaseSafe(propListData);
    return result;
}

static SecValidInfoRef _SecRevocationDbValidInfoForCertificate(SecRevocationDbRef this,
                                                               SecCertificateRef certificate,
                                                               CFDataRef issuerHash,
                                                               CFErrorRef *error) {
    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    __block int flags = 0;
    __block SecValidInfoFormat format = kSecValidInfoFormatUnknown;
    __block CFDataRef data = NULL;

    bool matched = false;
    int64_t groupId = 0;
    CFDataRef serial = NULL;
    CFDataRef certHash = NULL;
    SecValidInfoRef result = NULL;

#if TARGET_OS_OSX
    require(serial = SecCertificateCopySerialNumber(certificate, NULL), errOut);
#else
    require(serial = SecCertificateCopySerialNumber(certificate), errOut);
#endif
    require(certHash = SecCertificateCopySHA256Digest(certificate), errOut);

    require(groupId = _SecRevocationDbGroupIdForIssuerHash(this, issuerHash, &localError), errOut);

    /* Select the group record to determine flags and format. */
    ok &= SecDbPerformRead(this->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectGroupRecordSQL, &localError, ^bool(sqlite3_stmt *selectGroup) {
            ok = SecDbBindInt64(selectGroup, 1, groupId, &localError);
            ok &= SecDbStep(dbconn, selectGroup, &localError, ^(bool *stop) {
                flags = (int)sqlite3_column_int(selectGroup, 0);
                format = (SecValidInfoFormat)sqlite3_column_int(selectGroup, 1);
                uint8_t *p = (uint8_t *)sqlite3_column_blob(selectGroup, 2);
                if (p != NULL && format == kSecValidInfoFormatNto1) {
                    CFIndex length = (CFIndex)sqlite3_column_bytes(selectGroup, 2);
                    data = CFDataCreate(kCFAllocatorDefault, p, length);
                }
            });
            return ok;
        });
    });

    if (format == kSecValidInfoFormatUnknown) {
        /* No group record found for this issuer. */
    }
    else if (format == kSecValidInfoFormatSerial) {
        /* Look up certificate's serial number in the serials table. */
        matched = _SecRevocationDbSerialInGroup(this, serial, groupId, &localError);
    }
    else if (format == kSecValidInfoFormatSHA256) {
        /* Look up certificate's SHA-256 hash in the hashes table. */
        matched = _SecRevocationDbCertHashInGroup(this, certHash, groupId, &localError);
    }
    else if (format == kSecValidInfoFormatNto1) {
        /* Perform a Bloom filter match against the serial. If matched is false,
           then the cert is definitely not in the list. But if matched is true,
           we don't know for certain, so we would need to check OCSP. */
        matched = _SecRevocationDbSerialInFilter(this, serial, data);
    }

    if (matched) {
        /* Always return SecValidInfo for a matched certificate. */
        secdebug("validupdate", "Valid db matched cert: %@, format=%d, flags=%d",
                 certHash, format, flags);
        result = SecValidInfoCreate(format, flags, certHash, issuerHash);
    }
    else if ((flags & kSecValidInfoComplete) && (flags & kSecValidInfoAllowlist)) {
        /* Not matching against a complete whitelist is equivalent to revocation. */
        secdebug("validupdate", "Valid db did NOT match cert on allowlist: %@, format=%d, flags=%d",
                 certHash, format, flags);
        result = SecValidInfoCreate(format, flags, certHash, issuerHash);
    }

    if (result && SecIsAppleTrustAnchor(certificate, 0)) {
        /* Prevent a catch-22. */
        secdebug("validupdate", "Valid db match for Apple trust anchor: %@, format=%d, flags=%d",
                 certHash, format, flags);
        SecValidInfoRelease(result);
        result = NULL;
    }

errOut:
    (void) CFErrorPropagate(localError, error);
    CFReleaseSafe(data);
    CFReleaseSafe(certHash);
    CFReleaseSafe(serial);
    return result;
}

static SecValidInfoRef _SecRevocationDbCopyMatching(SecRevocationDbRef db,
                                                    SecCertificateRef certificate,
                                                    SecCertificateRef issuer) {
    SecValidInfoRef result = NULL;
    CFErrorRef error = NULL;
    CFDataRef issuerHash = NULL;

    require(certificate && issuer, errOut);
    require(issuerHash = SecCertificateCopySHA256Digest(issuer), errOut);

    result = _SecRevocationDbValidInfoForCertificate(db, certificate, issuerHash, &error);

errOut:
    CFReleaseSafe(issuerHash);
    CFReleaseSafe(error);
    return result;
}

static dispatch_queue_t _SecRevocationDbGetUpdateQueue(SecRevocationDbRef this) {
    return (this) ? this->update_queue : NULL;
}


/* Given a valid update dictionary, insert/replace or delete records
   in the revocation database. (This function is expected to be called only
   by the database maintainer, normally the system instance of trustd.)
*/
void SecRevocationDbApplyUpdate(CFDictionaryRef update, CFIndex version) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        _SecRevocationDbApplyUpdate(db, update, version);
    });
}

/* Set the schema version for the revocation database.
   (This function is expected to be called only by the database maintainer,
   normally the system instance of trustd.)
*/
void SecRevocationDbSetSchemaVersion(CFIndex db_version) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        _SecRevocationDbSetSchemaVersion(db, db_version);
    });
}

/* Set the next update value for the revocation database.
   (This function is expected to be called only by the database
   maintainer, normally the system instance of trustd. If the
   caller does not have write access, this is a no-op.)
*/
void SecRevocationDbSetNextUpdateTime(CFAbsoluteTime nextUpdate) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        _SecRevocationDbSetNextUpdateTime(db, nextUpdate);
    });
}

/* Return the next update value as a CFAbsoluteTime.
   If the value cannot be obtained, -1 is returned.
*/
CFAbsoluteTime SecRevocationDbGetNextUpdateTime(void) {
    __block CFAbsoluteTime result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = _SecRevocationDbGetNextUpdateTime(db, NULL);
    });
    return result;
}

/* Return the serial background queue for database updates.
   If the queue cannot be obtained, NULL is returned.
*/
dispatch_queue_t SecRevocationDbGetUpdateQueue(void) {
    __block dispatch_queue_t result = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = _SecRevocationDbGetUpdateQueue(db);
    });
    return result;
}

/* Remove all entries in the revocation database and reset its version to 0.
   (This function is expected to be called only by the database maintainer,
   normally the system instance of trustd.)
*/
void SecRevocationDbRemoveAllEntries(void) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        _SecRevocationDbRemoveAllEntries(db);
    });
}

/* === Public API === */

/* Given a certificate and its issuer, returns a SecValidInfoRef if the
   valid database contains matching info; otherwise returns NULL.
   Caller must release the returned SecValidInfoRef by calling
   SecValidInfoRelease when finished.
*/
SecValidInfoRef SecRevocationDbCopyMatching(SecCertificateRef certificate,
                                            SecCertificateRef issuer) {
    __block SecValidInfoRef result = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = _SecRevocationDbCopyMatching(db, certificate, issuer);
    });
    return result;
}

/* Return the current version of the revocation database.
   A version of 0 indicates an empty database which must be populated.
   If the version cannot be obtained, -1 is returned.
*/
CFIndex SecRevocationDbGetVersion(void) {
    __block CFIndex result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = (CFIndex)_SecRevocationDbGetVersion(db, NULL);
    });
    return result;
}

/* Return the current schema version of the revocation database.
   A version of 0 indicates an empty database which must be populated.
   If the schema version cannot be obtained, -1 is returned.
*/
CFIndex SecRevocationDbGetSchemaVersion(void) {
    __block CFIndex result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = (CFIndex)_SecRevocationDbGetSchemaVersion(db, NULL);
    });
    return result;
}
