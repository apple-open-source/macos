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

#include "ClientRequests.h"

#include "DNSCommon.h"
#include "uDNS.h"








#include "mdns_strict.h"

#define RecordTypeIsAddress(TYPE)   (((TYPE) == kDNSType_A) || ((TYPE) == kDNSType_AAAA))

extern mDNS mDNSStorage;

// Normally we append search domains only for queries with a single label that are not fully qualified. This can be
// overridden to apply search domains for queries (that are not fully qualified) with any number of labels e.g., moon,
// moon.cs, moon.cs.be, etc. - Mohan
mDNSBool AlwaysAppendSearchDomains = mDNSfalse;

// Control enabling optimistic DNS - Phil
mDNSBool EnableAllowExpired = mDNStrue;

typedef struct
{
    mDNSu32                 requestID;
    const domainname *      qname;
    mDNSu16                 qtype;
    mDNSu16                 qclass;
    mDNSInterfaceID         interfaceID;
    mDNSs32                 serviceID;
    mDNSu32                 flags;
    mDNSBool                appendSearchDomains;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
    mDNSBool                isInAppBrowserRequest;
    mDNSBool                useAAAAFallback;
    mDNSBool                persistWhenARecordsUnusable;

}   QueryRecordOpParams;

mDNSlocal void QueryRecordOpParamsInit(QueryRecordOpParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
    inParams->serviceID = -1;
}

mDNSlocal mStatus QueryRecordOpCreate(QueryRecordOp **outOp);
mDNSlocal void QueryRecordOpFree(QueryRecordOp *operation);
mDNSlocal mStatus QueryRecordOpStart(QueryRecordOp *inOp, const QueryRecordOpParams *inParams,
    QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSlocal void QueryRecordOpStop(QueryRecordOp *op);
mDNSlocal mDNSBool QueryRecordOpIsMulticast(const QueryRecordOp *op);
mDNSlocal void QueryRecordOpCallback(mDNS *m, DNSQuestion *inQuestion, const ResourceRecord *inAnswer,
    QC_result inAddRecord);
mDNSlocal void QueryRecordOpResetHandler(DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpStartQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpStopQuestion(DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpRestartUnicastQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion,
    const domainname *inSearchDomain);
mDNSlocal mStatus InterfaceIndexToInterfaceID(mDNSu32 inInterfaceIndex, mDNSInterfaceID *outInterfaceID);
mDNSlocal mDNSBool DomainNameIsSingleLabel(const domainname *inName);
mDNSlocal mDNSBool StringEndsWithDot(const char *inString);
mDNSlocal const domainname * NextSearchDomain(QueryRecordOp *inOp);

mDNSexport void GetAddrInfoClientRequestParamsInit(GetAddrInfoClientRequestParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
}

mDNSexport mStatus GetAddrInfoClientRequestStart(GetAddrInfoClientRequest *inRequest,
    const GetAddrInfoClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus             err;
    domainname          hostname;
    mDNSBool            appendSearchDomains;
    mDNSInterfaceID     interfaceID;
    DNSServiceFlags     flags;
    mDNSs32             serviceID;
    QueryRecordOpParams opParams;

    if (!MakeDomainNameFromDNSNameString(&hostname, inParams->hostnameStr))
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: bad hostname '" PRI_S "'", inParams->requestID, inParams->hostnameStr);
        err = mStatus_BadParamErr;
        goto exit;
    }

    if (inParams->protocols & ~((mDNSu32)(kDNSServiceProtocol_IPv4|kDNSServiceProtocol_IPv6)))
    {
        err = mStatus_BadParamErr;
        goto exit;
    }

    flags = inParams->flags;
    if (inParams->protocols == 0)
    {
        flags |= kDNSServiceFlagsSuppressUnusable;
        inRequest->protocols = kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6;
    }
    else
    {
        inRequest->protocols = inParams->protocols;
    }

    if (flags & kDNSServiceFlagsServiceIndex)
    {
        // NOTE: kDNSServiceFlagsServiceIndex flag can only be set for DNSServiceGetAddrInfo()
        LogInfo("GetAddrInfoClientRequestStart: kDNSServiceFlagsServiceIndex is SET by the client");

        // If kDNSServiceFlagsServiceIndex is SET, interpret the interfaceID as the serviceId and set the interfaceID to 0.
        serviceID   = (mDNSs32)inParams->interfaceIndex;
        interfaceID = mDNSNULL;
    }
    else
    {
        serviceID = -1;
        err = InterfaceIndexToInterfaceID(inParams->interfaceIndex, &interfaceID);
        if (err) goto exit;
    }
    inRequest->interfaceID = interfaceID;

    if (!StringEndsWithDot(inParams->hostnameStr) && (AlwaysAppendSearchDomains || DomainNameIsSingleLabel(&hostname)))
    {
        appendSearchDomains = mDNStrue;
    }
    else
    {
        appendSearchDomains = mDNSfalse;
    }
    QueryRecordOpParamsInit(&opParams);
    opParams.requestID              = inParams->requestID;
    opParams.qname                  = &hostname;
    opParams.qclass                 = kDNSClass_IN;
    opParams.interfaceID            = inRequest->interfaceID;
    opParams.serviceID              = serviceID;
    opParams.flags                  = flags;
    opParams.appendSearchDomains    = appendSearchDomains;
    opParams.effectivePID           = inParams->effectivePID;
    opParams.effectiveUUID          = inParams->effectiveUUID;
    opParams.peerUID                = inParams->peerUID;
    opParams.persistWhenARecordsUnusable = inParams->persistWhenARecordsUnusable;

    if (inRequest->protocols & kDNSServiceProtocol_IPv6)
    {
        err = QueryRecordOpCreate(&inRequest->op6);
        if (err) goto exit;

        opParams.qtype = kDNSType_AAAA;
        err = QueryRecordOpStart(inRequest->op6, &opParams, inResultHandler, inResultContext);
        if (err) goto exit;
    }
    if (inRequest->protocols & kDNSServiceProtocol_IPv4)
    {
        err = QueryRecordOpCreate(&inRequest->op4);
        if (err) goto exit;

        opParams.qtype = kDNSType_A;
        err = QueryRecordOpStart(inRequest->op4, &opParams, inResultHandler, inResultContext);
        if (err) goto exit;
    }
    err = mStatus_NoError;

exit:
    if (err) GetAddrInfoClientRequestStop(inRequest);
    return err;
}

mDNSexport void GetAddrInfoClientRequestStop(GetAddrInfoClientRequest *inRequest)
{
    if (inRequest->op4) QueryRecordOpStop(inRequest->op4);
    if (inRequest->op6) QueryRecordOpStop(inRequest->op6);


    if (inRequest->op4)
    {
        QueryRecordOpFree(inRequest->op4);
        inRequest->op4 = mDNSNULL;
    }
    if (inRequest->op6)
    {
        QueryRecordOpFree(inRequest->op6);
        inRequest->op6 = mDNSNULL;
    }
}

mDNSexport const domainname * GetAddrInfoClientRequestGetQName(const GetAddrInfoClientRequest *inRequest)
{
    if (inRequest->op4) return &inRequest->op4->q.qname;
    if (inRequest->op6) return &inRequest->op6->q.qname;
    return (const domainname *)"";
}

mDNSexport mDNSBool GetAddrInfoClientRequestIsMulticast(const GetAddrInfoClientRequest *inRequest)
{
    if ((inRequest->op4 && QueryRecordOpIsMulticast(inRequest->op4)) ||
        (inRequest->op6 && QueryRecordOpIsMulticast(inRequest->op6)))
    {
        return mDNStrue;
    }
    return mDNSfalse;
}

mDNSexport void QueryRecordClientRequestParamsInit(QueryRecordClientRequestParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
}

mDNSexport mStatus QueryRecordClientRequestStart(QueryRecordClientRequest *inRequest,
    const QueryRecordClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus             err;
    domainname          qname;
    mDNSInterfaceID     interfaceID;
    mDNSBool            appendSearchDomains;
    QueryRecordOpParams opParams;

    err = InterfaceIndexToInterfaceID(inParams->interfaceIndex, &interfaceID);
    if (err) goto exit;

    if (!MakeDomainNameFromDNSNameString(&qname, inParams->qnameStr))
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: bad domain name '" PRI_S "'", inParams->requestID, inParams->qnameStr);
        err = mStatus_BadParamErr;
        goto exit;
    }

    if (RecordTypeIsAddress(inParams->qtype) && !StringEndsWithDot(inParams->qnameStr) &&
        (AlwaysAppendSearchDomains || DomainNameIsSingleLabel(&qname)))
    {
        appendSearchDomains = mDNStrue;
    }
    else
    {
        appendSearchDomains = mDNSfalse;
    }
    QueryRecordOpParamsInit(&opParams);
    opParams.requestID              = inParams->requestID;
    opParams.qname                  = &qname;
    opParams.flags                  = inParams->flags;
    opParams.qtype                  = inParams->qtype;
    opParams.qclass                 = inParams->qclass;
    opParams.interfaceID            = interfaceID;
    opParams.appendSearchDomains    = appendSearchDomains;
    opParams.effectivePID           = inParams->effectivePID;
    opParams.effectiveUUID          = inParams->effectiveUUID;
    opParams.peerUID                = inParams->peerUID;
    opParams.useAAAAFallback   = inParams->useAAAAFallback;

    err = QueryRecordOpStart(&inRequest->op, &opParams, inResultHandler, inResultContext);

exit:
    if (err) QueryRecordClientRequestStop(inRequest);
    return err;
}

mDNSexport void QueryRecordClientRequestStop(QueryRecordClientRequest *inRequest)
{
    QueryRecordOpStop(&inRequest->op);

}

mDNSexport const domainname * QueryRecordClientRequestGetQName(const QueryRecordClientRequest *inRequest)
{
    return &inRequest->op.q.qname;
}

mDNSexport mDNSu16 QueryRecordClientRequestGetType(const QueryRecordClientRequest *inRequest)
{
    return inRequest->op.q.qtype;
}

mDNSexport mDNSBool QueryRecordClientRequestIsMulticast(QueryRecordClientRequest *inRequest)
{
    return (QueryRecordOpIsMulticast(&inRequest->op) ? mDNStrue : mDNSfalse);
}

mDNSlocal mStatus QueryRecordOpCreate(QueryRecordOp **outOp)
{
    mStatus err;
    QueryRecordOp *op;

    op = (QueryRecordOp *) mDNSPlatformMemAllocateClear(sizeof(*op));
    if (!op)
    {
        err = mStatus_NoMemoryErr;
        goto exit;
    }
    *outOp = op;
    err = mStatus_NoError;

exit:
    return err;
}

mDNSlocal void QueryRecordOpFree(QueryRecordOp *operation)
{
    mDNSPlatformMemFree(operation);
}

mDNSlocal void QueryRecordOpEventHandler(DNSQuestion *const inQuestion, const mDNSQuestionEvent event)
{
    QueryRecordOp *const op = (QueryRecordOp *)inQuestion->QuestionContext;
    switch (event)
    {
        case mDNSQuestionEvent_NoMoreExpiredRecords:
            if ((inQuestion->ExpRecordPolicy == mDNSExpiredRecordPolicy_UseCached) && op->gotExpiredCNAME)
            {
                // If an expired CNAME record was encountered, then rewind back to the original QNAME.
                QueryRecordOpStopQuestion(inQuestion);
                LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_INFO,
                    "[R%u->Q%u] Restarting question that got expired CNAMEs -- current name: " PRI_DM_NAME
                    ", original name: " PRI_DM_NAME ", type: " PUB_DNS_TYPE,
                    op->reqID, mDNSVal16(inQuestion->TargetQID), DM_NAME_PARAM(&inQuestion->qname), DM_NAME_PARAM(op->qname),
                    DNS_TYPE_PARAM(inQuestion->qtype));
                op->gotExpiredCNAME = mDNSfalse;
                AssignDomainName(&inQuestion->qname, op->qname);
                inQuestion->ExpRecordPolicy = mDNSExpiredRecordPolicy_Immortalize;
                const domainname *domain = mDNSNULL;
                // If we're appending search domains, the DNSQuestion needs to be retried without Optimistic DNS,
                // but with the search domain we just used, so restore the search list index to avoid skipping to
                // the next search domain.
                //
                // Note that when AppendSearchDomains is true, searchListIndex is the index of the next search
                // domain to try. So if searchListIndex is 0 or negative, that means that we are not currently in
                // the middle of iterating the search domain list, so no search domain needs to be restored. If
                // searchListIndex is greater than 0, then we're currently in the middle of iterating through the
                // search domain list, so the search domain that's currently in effect needs to be restored.
                if (inQuestion->AppendSearchDomains && (op->searchListIndex > 0))
                {
                    op->searchListIndex = op->searchListIndexLast;
                    domain = NextSearchDomain(op);
                }
                QueryRecordOpRestartUnicastQuestion(op, inQuestion, domain);
            }
            break;

        MDNS_COVERED_SWITCH_DEFAULT:
            break;
    }
}

#define VALID_MSAD_SRV_TRANSPORT(T) \
    (SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_tcp") || SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_udp"))
#define VALID_MSAD_SRV(Q) ((Q)->qtype == kDNSType_SRV && VALID_MSAD_SRV_TRANSPORT(SecondLabel(&(Q)->qname)))

mDNSlocal mStatus QueryRecordOpStart(QueryRecordOp *inOp, const QueryRecordOpParams *inParams,
    QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus                 err;
    DNSQuestion * const     q = &inOp->q;
    mDNSu32                 len;

    // Save the original qname.

    len = DomainNameLength(inParams->qname);
    inOp->qname = (domainname *) mDNSPlatformMemAllocate(len);
    if (!inOp->qname)
    {
        err = mStatus_NoMemoryErr;
        goto exit;
    }
    mDNSPlatformMemCopy(inOp->qname, inParams->qname, len);

    inOp->interfaceID          = inParams->interfaceID;
    inOp->reqID                = inParams->requestID;
    inOp->resultHandler        = inResultHandler;
    inOp->resultContext        = inResultContext;
    inOp->useAAAAFallback      = inParams->useAAAAFallback;
    // Set up DNSQuestion.
    q->flags = inParams->flags;
    // If appending search domains, clear the kDNSServiceFlagsPathEvaluationDone flag since path evaluation needs to be
    // performed for each FQDN that's the result of appending a search domain from the search domain list to the
    // original PQDN in the client request. Path evaluation takes the FQDN being queried as a parameter, so it's
    // necessary to perform a new path evaluation when dealing with a new FQDN so that the appropriate DNS service is
    // used.
    if (inParams->appendSearchDomains)
    {
        q->flags &= ~((DNSServiceFlags)kDNSServiceFlagsPathEvaluationDone);
    }
    mDNSBool allowExpiredAnswers = EnableAllowExpired && (q->flags & kDNSServiceFlagsAllowExpiredAnswers);
    if (allowExpiredAnswers)
    {
        q->ExpRecordPolicy     = mDNSExpiredRecordPolicy_UseCached;
    }
    else
    {
        q->ExpRecordPolicy     = mDNSExpiredRecordPolicy_DoNotUse;
    }
    q->ServiceID                  = inParams->serviceID;
    q->InterfaceID                = inParams->interfaceID;
    AssignDomainName(&q->qname, inParams->qname);
    q->qtype                      = inParams->qtype;
    q->qclass                     = inParams->qclass;
    q->LongLived                  = (q->flags & kDNSServiceFlagsLongLivedQuery) != 0;
    q->ForceMCast                 = (q->flags & kDNSServiceFlagsForceMulticast) != 0;
    q->ReturnIntermed             = (q->flags & kDNSServiceFlagsReturnIntermediates) != 0;
    q->SuppressUnusable           = (q->flags & kDNSServiceFlagsSuppressUnusable) != 0;
    q->TimeoutQuestion            = (q->flags & kDNSServiceFlagsTimeout) != 0;
    q->UseBackgroundTraffic       = (q->flags & kDNSServiceFlagsBackgroundTrafficClass) != 0;
    q->AppendSearchDomains        = inParams->appendSearchDomains;
    q->PersistWhenRecordsUnusable = (inParams->qtype == kDNSType_A) && inParams->persistWhenARecordsUnusable;
    q->InitialCacheMiss     = mDNSfalse;


    q->pid              = inParams->effectivePID;
    if (inParams->effectiveUUID)
    {
        mDNSPlatformMemCopy(q->uuid, inParams->effectiveUUID, UUID_SIZE);
    }
    q->euid             = inParams->peerUID;
    q->request_id       = inParams->requestID;
    q->QuestionCallback = QueryRecordOpCallback;
    q->ResetHandler     = QueryRecordOpResetHandler;
    q->EventHandler     = QueryRecordOpEventHandler;

    // For single label queries that are not fully qualified, look at /etc/hosts, cache and try search domains before trying
    // them on the wire as a single label query. - Mohan

    if (q->AppendSearchDomains && DomainNameIsSingleLabel(inOp->qname)) q->InterfaceID = mDNSInterface_LocalOnly;


    err = QueryRecordOpStartQuestion(inOp, q);
    if (err) goto exit;


    err = mStatus_NoError;

exit:
    if (err) QueryRecordOpStop(inOp);
    return err;
}

mDNSlocal void QueryRecordOpStop(QueryRecordOp *op)
{
    if (op->q.QuestionContext)
    {
        QueryRecordOpStopQuestion(&op->q);
    }
    if (op->qname)
    {
        mDNSPlatformMemFree(op->qname);
        op->qname = mDNSNULL;
    }
}

mDNSlocal mDNSBool QueryRecordOpIsMulticast(const QueryRecordOp *op)
{
    return ((mDNSOpaque16IsZero(op->q.TargetQID) && (op->q.ThisQInterval > 0)) ? mDNStrue : mDNSfalse);
}

// GetTimeNow is a callback-safe alternative to mDNS_TimeNow(), which expects to be called with m->mDNS_busy == 0.
mDNSlocal mDNSs32 GetTimeNow(mDNS *m)
{
    mDNSs32 time;
    mDNS_Lock(m);
    time = m->timenow;
    mDNS_Unlock(m);
    return time;
}

mDNSlocal void QueryRecordOpCallback(mDNS *m, DNSQuestion *inQuestion, const ResourceRecord *inAnswer, QC_result inAddRecord)
{
    mStatus                     resultErr;
    QueryRecordOp *const        op = (QueryRecordOp *)inQuestion->QuestionContext;
    const domainname *          domain;

    // The mDNSExpiredRecordPolicy_UseCached policy unconditionally provides us with CNAMEs. So if the client
    // doesn't want intermediate results, which includes CNAMEs, then don't provide them with CNAMEs unless the
    // client request was specifically for CNAME records.
    if (inQuestion->ExpRecordPolicy == mDNSExpiredRecordPolicy_UseCached)
    {
        if ((inAddRecord == QC_add) && (inAnswer->rrtype == kDNSType_CNAME))
        {
            if (inAnswer->mortality == Mortality_Ghost)
            {
                op->gotExpiredCNAME = mDNStrue;
            }
            if (!(inQuestion->ReturnIntermed || (inQuestion->qtype == kDNSType_CNAME)))
            {
                goto exit;
            }
        }
    }
    if (inAddRecord == QC_suppressed)
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEBUG,
               "[R%u] QueryRecordOpCallback: Suppressed question " PRI_DM_NAME " (" PUB_S ")",
               op->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype));

        resultErr = kDNSServiceErr_NoSuchRecord;
    }
    else if (inAnswer->RecordType == kDNSRecordTypePacketNegative)
    {
        if (inQuestion->TimeoutQuestion && ((GetTimeNow(m) - inQuestion->StopTime) >= 0))
        {
            LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
                   "[R%u] QueryRecordOpCallback: Question " PRI_DM_NAME " (" PUB_S ") timing out, InterfaceID %p",
                   op->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype),
                   inQuestion->InterfaceID);
            resultErr = kDNSServiceErr_Timeout;
        }
        else
        {
            if (inQuestion->AppendSearchDomains && (op->searchListIndex >= 0) && inAddRecord)
            {
                domain = NextSearchDomain(op);
                if (domain || DomainNameIsSingleLabel(op->qname))
                {
                    QueryRecordOpStopQuestion(inQuestion);
                    QueryRecordOpRestartUnicastQuestion(op, inQuestion, domain);
                    goto exit;
                }
            }
            if (op->useAAAAFallback && (inQuestion->qtype == kDNSType_AAAA) && (inAnswer->rcode != kDNSFlag1_RC_NXDomain))
            {
                LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEBUG,
                    "[R%u] Restarting question for " PRI_DM_NAME " AAAA record as question for A record (RCODE %d)",
                    op->reqID, DM_NAME_PARAM(&inQuestion->qname), inAnswer->rcode);
                QueryRecordOpStopQuestion(inQuestion);
                inQuestion->qtype = kDNSType_A;
                QueryRecordOpStartQuestion(op, inQuestion);
                goto exit;
            }
            resultErr = kDNSServiceErr_NoSuchRecord;
        }
    }
    else
    {
        resultErr = kDNSServiceErr_NoError;
    }



    // The result handler is allowed to stop the client request, so it's not safe to touch the DNSQuestion or
    // the QueryRecordOp unless m->CurrentQuestion still points to this DNSQuestion.
    if (op->resultHandler)
    {
        const mDNSBool expired = (inAddRecord == QC_add) && (op->gotExpiredCNAME || (inAnswer->mortality == Mortality_Ghost));
        op->resultHandler(m, inQuestion, inAnswer, expired, inAddRecord, resultErr, op->resultContext);
    }
    if (m->CurrentQuestion == inQuestion)
    {
        if (resultErr == kDNSServiceErr_Timeout) QueryRecordOpStopQuestion(inQuestion);
    }


exit:
    return;
}

mDNSlocal void QueryRecordOpResetHandler(DNSQuestion *inQuestion)
{
    QueryRecordOp *const        op = (QueryRecordOp *)inQuestion->QuestionContext;

    AssignDomainName(&inQuestion->qname, op->qname);
    if (inQuestion->AppendSearchDomains && DomainNameIsSingleLabel(op->qname))
    {
        inQuestion->InterfaceID = mDNSInterface_LocalOnly;
    }
    else
    {
        inQuestion->InterfaceID = op->interfaceID;
    }
    op->searchListIndex = 0;
    op->searchListIndexLast = 0;
}

mDNSlocal mStatus QueryRecordOpStartQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion)
{
    mStatus     err;

    inQuestion->QuestionContext = inOp;
    err = mDNS_StartQuery(&mDNSStorage, inQuestion);
    if (err)
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: QueryRecordOpStartQuestion mDNS_StartQuery for " PRI_DM_NAME " " PUB_S " failed with error %d",
               inOp->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype), err);
        inQuestion->QuestionContext = mDNSNULL;
    }
    return err;
}

mDNSlocal mStatus QueryRecordOpStopQuestion(DNSQuestion *inQuestion)
{
    mStatus     err;

    err = mDNS_StopQuery(&mDNSStorage, inQuestion);
    inQuestion->QuestionContext = mDNSNULL;
    return err;
}

mDNSlocal mStatus QueryRecordOpRestartUnicastQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion,
    const domainname *inSearchDomain)
{
    mStatus     err;

    inQuestion->InterfaceID = inOp->interfaceID;
    AssignDomainName(&inQuestion->qname, inOp->qname);
    if (inSearchDomain) AppendDomainName(&inQuestion->qname, inSearchDomain);
    if (SameDomainLabel(LastLabel(&inQuestion->qname), (const mDNSu8 *)&localdomain))
    {
        inQuestion->IsUnicastDotLocal = mDNStrue;
    }
    else
    {
        inQuestion->IsUnicastDotLocal = mDNSfalse;
    }
    err = QueryRecordOpStartQuestion(inOp, inQuestion);
    return err;
}

mDNSlocal mStatus InterfaceIndexToInterfaceID(mDNSu32 inInterfaceIndex, mDNSInterfaceID *outInterfaceID)
{
    mStatus             err;
    mDNSInterfaceID     interfaceID;

    interfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, inInterfaceIndex);


    *outInterfaceID = interfaceID;
    err = mStatus_NoError;

    return err;
}

mDNSlocal mDNSBool DomainNameIsSingleLabel(const domainname *inName)
{
    const mDNSu8 *const     label = inName->c;
    return (((label[0] != 0) && (label[1 + label[0]] == 0)) ? mDNStrue : mDNSfalse);
}

mDNSlocal mDNSBool StringEndsWithDot(const char *inString)
{
    const char *        ptr;
    mDNSu32             escapeCount;
    mDNSBool            result;

    // Loop invariant: escapeCount is the number of consecutive escape characters that immediately precede *ptr.
    // - If escapeCount is even, then *ptr is immediately preceded by escapeCount / 2 consecutive literal backslash
    //   characters, so *ptr is not escaped.
    // - If escapeCount is odd, then *ptr is immediately preceded by (escapeCount - 1) / 2 consecutive literal backslash
    //   characters followed by an escape character, so *ptr is escaped.
    escapeCount = 0;
    result = mDNSfalse;
    for (ptr = inString; *ptr != '\0'; ptr++)
    {
        if (*ptr == '\\')
        {
            escapeCount++;
        }
        else
        {
            if ((*ptr == '.') && (ptr[1] == '\0'))
            {
                if ((escapeCount % 2) == 0) result = mDNStrue;
                break;
            }
            escapeCount = 0;
        }
    }
    return result;
}

mDNSlocal const domainname * NextSearchDomain(QueryRecordOp *inOp)
{
    const domainname *      domain;

    inOp->searchListIndexLast = inOp->searchListIndex;
    while ((domain = uDNS_GetNextSearchDomain(inOp->interfaceID, &inOp->searchListIndex, mDNSfalse)) != mDNSNULL)
    {
        if ((DomainNameLength(inOp->qname) - 1 + DomainNameLength(domain)) <= MAX_DOMAIN_NAME) break;
    }
    if (!domain) inOp->searchListIndex = -1;
    return domain;
}



