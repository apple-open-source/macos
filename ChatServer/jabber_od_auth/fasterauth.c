/*
 *  fasterauth.c
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

/**
 * A class that wraps high-level Directory Service calls needed by the
 * CalDAV server.
 **
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/


#include <sys/types.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include "fasterauth.h"
#include "dserr.h"

/* this code is not thread safe:
 *
 *   . dunno if the tDirReference and/or tDirNodeReference are thread safe
 *   . RemoveCachedFasterDirectoryService operates on a FasterDirectoryService without a mutex
 *
 * probably the way to fix that would be to create a dictionary of nodes per thread
 */

typedef struct FasterDirectoryService {
    char                 *mNodeName;
    tDirReference         mDir;
    tDirNodeReference     mNode;
} FasterDirectoryService;

static CFDictionaryValueCallBacks  kNoOpValueCallBacks = { 0 };

static CFMutableDictionaryRef nodes = NULL;

const int cBufferSize = 32 * 1024;        // 32K buffer for Directory Services operations


static int CreateFasterDirectoryService(FasterDirectoryService **out, char *nodename);
static int DeleteFasterDirectoryService(FasterDirectoryService **out);

static int GetCachedFasterDirectoryService(char *nodename, FasterDirectoryService **out);
static int RemoveCachedFasterDirectoryService(char *nodename);

static int GetFasterDirectoryServiceDirReference(FasterDirectoryService *svc, tDirReference *out);
static int GetFasterDirectoryServiceDirNodeReference(FasterDirectoryService *svc, tDirNodeReference *out);
static int GetFasterDirectoryServiceDataBufferPtr(FasterDirectoryService *svc, tDataBufferPtr *out);


#define CF_SAFE_RELEASE(cfobj) \
    do { if ((cfobj) != NULL) CFRelease((cfobj)); cfobj = NULL; } while (0)




int
FasterAuthentication(char *nodename, char *user,
                     char *challenge, char *response,
                     char **serverresponse)
{
    int retval = -1;
    tDirNodeReference node = 0;
    tDataNodePtr authType = NULL;
    tDataBufferPtr authData = NULL;
    tContextData context = 0;
    FasterDirectoryService *svc = 0;
    tDirReference dir = 0;
    tDataBufferPtr data = NULL;
    tDirStatus dirStatus = eDSNoErr;

    if (nodename == 0 || user == 0 || challenge == 0 || response == 0 || serverresponse == 0)
        goto done;

    *serverresponse = 0;

    if (GetCachedFasterDirectoryService(nodename, &svc) != 0)
        goto done;

    if (GetFasterDirectoryServiceDirReference(svc, &dir) != 0)
        goto done;

    // First, specify the type of authentication.
    authType = dsDataNodeAllocateString(dir, kDSStdAuthDIGEST_MD5);

    // Build input data
    //  Native authentication is a one step authentication scheme.
    //  Step 1
    //      Send: <length><user>
    //            <length><challenge>
    //            <length><response>
    //   Receive: success or failure.
    UInt32 aDataBufSize = sizeof(UInt32) + strlen(user) +
                          sizeof(UInt32) + strlen(challenge) +
                          sizeof(UInt32) + strlen(response);
    authData = dsDataBufferAllocate(dir, aDataBufSize);
    if (authData == NULL)
        goto done;
    
    // Fill the buffer
    dsFillAuthBuffer(authData, 3,
                     strlen(user), user,
                     strlen(challenge), challenge,
                     strlen(response), response);

    if (GetFasterDirectoryServiceDirNodeReference(svc, &node) != 0)
        goto done;

    data = dsDataBufferAllocate(dir, cBufferSize);
    if (data == NULL)
        goto done;

    // Do authentication
    dirStatus = dsDoDirNodeAuth(node, authType, true, authData, data, &context);
    if (dirStatus != eDSNoErr)
        goto done;

    if (data->fBufferLength >= data->fBufferSize)
        goto done;

    data->fBufferData[data->fBufferLength] = '\0'; // make sure it's null terminated
    *serverresponse = strdup(&data->fBufferData[4]);
    if (*serverresponse == 0)
        goto done;

    retval = 0;
done:
    if (data != NULL)
        dsDataBufferDeAllocate(dir, data);
    data = NULL;
    if (authData != NULL)
        dsDataBufferDeAllocate(dir, authData);
    authData = NULL;
    if (authType != NULL)
        dsDataNodeDeAllocate(dir, authType);
    authType = NULL;

    if (retval != 0) { 
        if (! IS_EXPECTED_DS_ERROR(dirStatus)) {
            /* something went wrong, invalidate that service */
            (void)RemoveCachedFasterDirectoryService(nodename);
        }
    }

    return retval;
}

int
GetCachedFasterDirectoryService(char *nodename, FasterDirectoryService **out)
{
    int retval = -1;
    FasterDirectoryService *svc = 0;
    CFStringRef cfNodeName = NULL;

    *out = 0;

    if (nodes == NULL) {
        nodes = CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kNoOpValueCallBacks);

        if (nodes == NULL)
            goto done;
    }

    cfNodeName = CFStringCreateWithCString(kCFAllocatorDefault, nodename, kCFStringEncodingUTF8);
    if (cfNodeName == NULL)
        goto done;

    svc = (FasterDirectoryService*)CFDictionaryGetValue(nodes, cfNodeName);
    if (svc != 0) {
        /* verify that the cached directory reference is still good */
        if (dsVerifyDirRefNum(svc->mDir) != eDSNoErr) {
            /* it's no longer good, destroy it so that it'll be re-created below */
            svc = 0;
            RemoveCachedFasterDirectoryService(nodename);
        }
    }

    if (svc == 0) {
        if (CreateFasterDirectoryService(&svc, nodename) != 0)
            goto done;

        CFDictionarySetValue(nodes, cfNodeName, svc);
    }

    *out = svc;

    retval = 0;
done:
    CF_SAFE_RELEASE(cfNodeName);
    return retval;
}

int
RemoveCachedFasterDirectoryService(char *nodename)
{
    int retval = -1;
    CFStringRef cfNodeName = NULL;
    FasterDirectoryService *svc = 0;

    cfNodeName = CFStringCreateWithCString(kCFAllocatorDefault, nodename, kCFStringEncodingUTF8);
    if (cfNodeName == NULL)
        goto done;

    svc = (FasterDirectoryService*)CFDictionaryGetValue(nodes, cfNodeName);

    CFDictionaryRemoveValue(nodes, cfNodeName);

    /* now it's been removed from the dictionary, so it's safe to delete it */

    (void)DeleteFasterDirectoryService(&svc);

    retval = 0;
done:
    CF_SAFE_RELEASE(cfNodeName);
    return retval;
}

int
CreateFasterDirectoryService(FasterDirectoryService **out, char *nodename)
{
    int retval = -1;
    FasterDirectoryService *svc = 0;

    *out = 0;

    svc = (FasterDirectoryService*)calloc(1, sizeof(*svc));
    if (svc == 0)
        goto done;

    svc->mNodeName = strdup(nodename);
    if (svc->mNodeName == 0)
        goto done;
   
    *out = svc;

    retval = 0;
done:
    return retval;
}

int
DeleteFasterDirectoryService(FasterDirectoryService **out)
{
    int retval = -1;
    FasterDirectoryService *svc = 0;

    if (out == 0)
        goto done;

    if (*out != 0) {
        svc = *out;

        if (svc->mNodeName != 0)
            free(svc->mNodeName);

        if (svc->mNode != 0)
            dsCloseDirNode(svc->mNode);

        if (svc->mDir != 0)
            dsCloseDirService(svc->mDir);

        memset(svc, 0, sizeof(*svc));

        free(svc);

        *out = 0;
    }

    retval = 0;
done:
    return retval;
}

int
GetFasterDirectoryServiceDirReference(FasterDirectoryService *svc, tDirReference *out)
{
    int retval = -1;

    if (svc->mDir == 0) {
        tDirStatus dirStatus = dsOpenDirService(&svc->mDir);
        if (dirStatus != eDSNoErr) {
            svc->mDir = 0;
            goto done;
        }
    }

    *out = svc->mDir;

    retval = 0;
done:
    return retval;
}

int
GetFasterDirectoryServiceDirNodeReference(FasterDirectoryService *svc, tDirNodeReference *out)
{
    int retval = -1;
    tDirReference dir = 0;
    tDirStatus dirStatus = eDSNoErr;
    tDataListPtr nodePath = NULL;

    if (svc->mNode == 0) {
        if (GetFasterDirectoryServiceDirReference(svc, &dir) != 0)
            goto done;

        nodePath = dsDataListAllocate(dir);
        if (nodePath == NULL)
            goto done;

        dirStatus = dsBuildListFromPathAlloc(dir, nodePath, svc->mNodeName, "/");
        if (dirStatus != eDSNoErr)
            goto done;

        dirStatus = dsOpenDirNode(dir, nodePath, &svc->mNode);
        if (dirStatus != eDSNoErr)
            goto done;
    }

    *out = svc->mNode;

    retval = 0;
done:
    if (nodePath != NULL) {
        dirStatus = dsDataListDeallocate(dir, nodePath);
        free(nodePath);
        nodePath = NULL;
    }

    return retval;
}

