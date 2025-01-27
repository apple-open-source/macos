/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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


//
// cssmtrust - CSSM layer Trust (TP) related objects.
//
#ifndef _H_CSSMTRUST
#define _H_CSSMTRUST

#include <security_cdsa_utilities/cssmbridge.h>
#include <security_cdsa_utilities/cssmcert.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_cdsa_utilities/cssmdb.h>


namespace Security {


//
// A TP "POLICYINFO" structure, essentially an OID/Data pair.
//
class PolicyInfo : public PodWrapper<PolicyInfo, CSSM_TP_POLICYINFO> {
public:
    uint32 count() const				{ return NumberOfPolicyIds; }
    uint32 &count()						{ return NumberOfPolicyIds; }
    CssmField *policies() const			{ return CssmField::overlay(PolicyIds); }
    CssmField * &policies()				{ return CssmField::overlayVar(PolicyIds); }
    void *control() const				{ return PolicyControl; }
    
    CssmField &operator [] (uint32 ix)
    { assert(ix < count()); return policies()[ix]; }
    
    void setPolicies(uint32 n, CSSM_FIELD *p)
    { count() = n; policies() = CssmField::overlay(p); }
};


//
// TP caller authentication contexts
//
class TPCallerAuth : public PodWrapper<TPCallerAuth, CSSM_TP_CALLERAUTH_CONTEXT> {
public:
    CSSM_TP_STOP_ON stopCriterion() const	{ return VerificationAbortOn; }
    void stopCriterion(CSSM_TP_STOP_ON stop) { VerificationAbortOn = stop; }
    
    CSSM_TIMESTRING time() const			{ return VerifyTime; }
    void time(CSSM_TIMESTRING newTime)		{ VerifyTime = newTime; }
    
    PolicyInfo &policies()					{ return PolicyInfo::overlay(Policy); }
    const PolicyInfo &policies() const		{ return PolicyInfo::overlay(Policy); }
    void setPolicies(uint32 n, CSSM_FIELD *p) { policies().setPolicies(n, p); }
    
    AccessCredentials *creds() const
    { return AccessCredentials::optional(CallerCredentials); }
    void creds(AccessCredentials *newCreds)	{ CallerCredentials = newCreds; }
    
    uint32 anchorCount() const				{ return NumberOfAnchorCerts; }
    uint32 &anchorCount()					{ return NumberOfAnchorCerts; }
    CssmData *anchors() const				{ return CssmData::overlay(AnchorCerts); }
    CssmData * &anchors()					{ return CssmData::overlayVar(AnchorCerts); }
	
	CssmDlDbList *dlDbList() const			{ return CssmDlDbList::overlay(DBList); }
	CssmDlDbList * &dlDbList()				{ return CssmDlDbList::overlayVar(DBList); }
};


//
// TP Verify Contexts - a monster collection of possibly useful stuff
// when verifying a certificate against trust policies
//
class TPVerifyContext : public PodWrapper<TPVerifyContext, CSSM_TP_VERIFY_CONTEXT> {
public:
    CSSM_TP_ACTION action() const		{ return Action; }
    CssmData &actionData()				{ return CssmData::overlay(ActionData); }
    const CssmData &actionData() const	{ return CssmData::overlay(ActionData); }
    
    // set and reference the CallerAuth component
    TPCallerAuth &callerAuth() const	{ return TPCallerAuth::required(Cred); }
    operator TPCallerAuth &() const		{ return callerAuth(); }
    TPCallerAuth *callerAuthPtr() const	{ return TPCallerAuth::optional(Cred); }
    void callerAuthPtr(CSSM_TP_CALLERAUTH_CONTEXT *p) { Cred = p; }
    
    // forward CallerAuth operations
    
    CSSM_TP_STOP_ON stopCriterion() const { return callerAuth().stopCriterion(); }
    void stopCriterion(CSSM_TP_STOP_ON stop) { return callerAuth().stopCriterion(stop); }
    PolicyInfo &policies() const		{ return callerAuth().policies(); }
    void setPolicies(uint32 n, CSSM_FIELD *p) { policies().setPolicies(n, p); }
    CSSM_TIMESTRING time() const		{ return callerAuth().time(); }
    void time(CSSM_TIMESTRING newTime)	{ return callerAuth().time(newTime); }
    AccessCredentials *creds() const	{ return callerAuth().creds(); }
    void creds(AccessCredentials *newCreds) const { return callerAuth().creds(newCreds); }
    uint32 anchorCount() const			{ return callerAuth().anchorCount(); }
    uint32 &anchorCount()				{ return callerAuth().anchorCount(); }
    CssmData *anchors() const			{ return callerAuth().anchors(); }
    CssmData * &anchors()				{ return callerAuth().anchors(); }
    void anchors(uint32 count, CSSM_DATA *vector)
    { anchorCount() = count; anchors() = CssmData::overlay(vector); }
	void setDlDbList(uint32 n, CSSM_DL_DB_HANDLE *list)
	{ callerAuth().dlDbList()->setDlDbList(n, list); }
};


//
// The result of a (raw) TP trust verification call
//
class TPEvidence : public PodWrapper<TPEvidence, CSSM_EVIDENCE> {
public:
    CSSM_EVIDENCE_FORM form() const		{ return EvidenceForm; }
	void *data() const					{ return Evidence; }
    operator void *() const				{ return data(); }
    
    template <class T>
    T *as() const { return reinterpret_cast<T *>(Evidence); }
};

class TPVerifyResult : public PodWrapper<TPVerifyResult, CSSM_TP_VERIFY_CONTEXT_RESULT> {
public:
    uint32 count() const				{ return NumberOfEvidences; }
    const TPEvidence &operator [] (uint32 ix) const
    { assert(ix < count()); return TPEvidence::overlay(Evidence[ix]); }
};


//
// A PodWrapper for Apple's TP supporting-evidence structure
//
class TPEvidenceInfo : public PodWrapper<TPEvidenceInfo, CSSM_TP_APPLE_EVIDENCE_INFO> {
public:
    CSSM_TP_APPLE_CERT_STATUS status() const	{ return StatusBits; }
    CSSM_TP_APPLE_CERT_STATUS status(CSSM_TP_APPLE_CERT_STATUS flags) const
    { return status() & flags; }
    
    uint32 index() const		{ return Index; }
    const CssmDlDbHandle &dldb() const { return CssmDlDbHandle::overlay(DlDbHandle); }
    CSSM_DB_UNIQUE_RECORD_PTR recordId() const { return UniqueRecord; }
    
    uint32 codes() const		{ return NumStatusCodes; }
    CSSM_RETURN operator [] (uint32 ix)
    { assert(ix < NumStatusCodes); return StatusCodes[ix]; }
	
	void destroy(Allocator &allocator);
};


//
// Walkers
//
namespace DataWalkers {




}	// end namespace DataWalkers
}	// end namespace Security

#endif //_H_CSSMTRUST
