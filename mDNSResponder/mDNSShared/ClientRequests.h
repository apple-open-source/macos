/*
 * Copyright (c) 2018-2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ClientRequests_h
#define __ClientRequests_h

#include "mDNSEmbeddedAPI.h"
#include "dns_sd_internal.h"


typedef void (*QueryRecordResultHandler)(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer,
    mDNSBool expired, QC_result AddRecord, DNSServiceErrorType error, void *context);

typedef struct
{
    DNSQuestion                 q;                      // DNSQuestion for record query.
    domainname *                qname;                  // Name of the original record.
    mDNSInterfaceID             interfaceID;            // Interface over which to perform query.
    QueryRecordResultHandler    resultHandler;          // Handler for query record operation results.
    void *                      resultContext;          // Context to pass to result handler.
    mDNSu32                     reqID;                  // Client request ID.
    int                         searchListIndex;        // Index that indicates the next search domain to try.
    int                         searchListIndexLast;    // Value of searchListIndex prior to calling NextSearchDomain().
    mDNSBool                    useAAAAFallback;        // If a AAAA question gets a negative answer, it's restarted as an A.
    mDNSBool                    gotExpiredCNAME;        // True is an expired CNAME record was encountered.

}   QueryRecordOp;

typedef struct
{
    mDNSInterfaceID     interfaceID;    // InterfaceID being used for query record operations.
    mDNSu32             protocols;      // Protocols (IPv4, IPv6) specified by client.
    QueryRecordOp *     op4;            // Query record operation object for A record.
    QueryRecordOp *     op6;            // Query record operation object for AAAA record.

}   GetAddrInfoClientRequest;
mdns_compile_time_max_size_check(GetAddrInfoClientRequest, 32);

typedef struct
{
    QueryRecordOp       op; // Query record operation object.

}   QueryRecordClientRequest;
mdns_compile_time_max_size_check(QueryRecordClientRequest, 816);

typedef struct
{
    mDNSu32                 requestID;
    const char *            hostnameStr;
    mDNSu32                 interfaceIndex;
    DNSServiceFlags         flags;
    mDNSu32                 protocols;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
    mDNSBool                persistWhenARecordsUnusable;

}   GetAddrInfoClientRequestParams;

typedef struct
{
    mDNSu32                 requestID;
    const char *            qnameStr;
    mDNSu32                 interfaceIndex;
    DNSServiceFlags         flags;
    mDNSu16                 qtype;
    mDNSu16                 qclass;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
    mDNSBool                useAAAAFallback;

}   QueryRecordClientRequestParams;

#ifdef __cplusplus
extern "C" {
#endif

mDNSexport void GetAddrInfoClientRequestParamsInit(GetAddrInfoClientRequestParams *inParams);
mDNSexport mStatus GetAddrInfoClientRequestStart(GetAddrInfoClientRequest *inRequest,
    const GetAddrInfoClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSexport void GetAddrInfoClientRequestStop(GetAddrInfoClientRequest *inRequest);
mDNSexport const domainname * GetAddrInfoClientRequestGetQName(const GetAddrInfoClientRequest *inRequest);
mDNSexport mDNSBool GetAddrInfoClientRequestIsMulticast(const GetAddrInfoClientRequest *inRequest);

mDNSexport void QueryRecordClientRequestParamsInit(QueryRecordClientRequestParams *inParams);
mDNSexport mStatus QueryRecordClientRequestStart(QueryRecordClientRequest *inRequest,
    const QueryRecordClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSexport void QueryRecordClientRequestStop(QueryRecordClientRequest *inRequest);
mDNSexport const domainname * QueryRecordClientRequestGetQName(const QueryRecordClientRequest *inRequest);
mDNSexport mDNSu16 QueryRecordClientRequestGetType(const QueryRecordClientRequest *inRequest);
mDNSexport mDNSBool QueryRecordClientRequestIsMulticast(QueryRecordClientRequest *inRequest);

#ifdef __cplusplus
}
#endif

#endif  // __ClientRequests_h
