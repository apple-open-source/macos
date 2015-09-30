/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// mds_standard - standard-defined MDS record types.
//
// These are the C++ record types corresponding to standard and Apple-defined
// MDS relations. Note that not all standard fields are included; only those
// of particular interest to the implementation. Feel free to add field functions
// as needed.
//

#ifndef _H_CDSA_CLIENT_MDS_STANDARD
#define _H_CDSA_CLIENT_MDS_STANDARD

#include <security_cdsa_client/mdsclient.h>


namespace Security {
namespace MDSClient {


//
// The CDSA Common table (one record per module)
//
class Common : public Record {
public:
	Common();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_COMMON_RECORDTYPE;
	
	string moduleID() const;
	string moduleName() const;
	string path() const;
	string description() const;
	bool dynamic() const;
	bool singleThreaded() const;
	CSSM_SERVICE_MASK serviceMask() const;
	
public:
	//
	// "Link in" a Common into another record, whose attributes()[0] is the ModuleID
	//
	class Carrier {
	public:
		virtual ~Carrier();
		
		string moduleName() const			{ return common().moduleName(); }
		string path() const					{ return common().path(); }
		string description() const			{ return common().description(); }
		bool dynamic() const				{ return common().dynamic(); }
		bool singleThreaded() const			{ return common().singleThreaded(); }
		CSSM_SERVICE_MASK serviceMask() const { return common().serviceMask(); }
	
	private:
		mutable RefPointer<Common> mCommon;
		
		Common &common() const;
	};
};


//
// PrimaryRecord shapes the "common head" of all MDS primary relations
//
class PrimaryRecord : public Record, public Common::Carrier {
public:
	PrimaryRecord(const char * const * names);

	string moduleID() const;
	uint32 subserviceID() const;
	string moduleName() const;
	string productVersion() const;
	string vendor() const;
};


//
// The CSP Primary relation
//
class CSP : public PrimaryRecord {
public:
	CSP();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE;

	uint32 cspType() const;
	CSSM_CSP_FLAGS cspFlags() const;
};


//
// The CSP Capabilities relation
//
class CSPCapabilities : public Record, public Common::Carrier {
public:
	CSPCapabilities();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE;

	string moduleID() const;
	uint32 subserviceID() const;
	uint32 contextType() const;
	uint32 algorithm() const;
	uint32 group() const;
	uint32 attribute() const;
	string description() const;
};


//
// The CSP "smartcard token" relation
//
class SmartcardInfo : public Record, public Common::Carrier {
public:
	SmartcardInfo();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_CSP_SC_INFO_RECORDTYPE;
	
	string moduleID() const;
	uint32 subserviceID() const;
	string description() const;
	string vendor() const;
	string version() const;
	string firmware() const;
	CSSM_SC_FLAGS flags() const;
	CSSM_SC_FLAGS customFlags() const;
	string serial() const;
};


//
// The DL Primary relation
//
class DL : public PrimaryRecord {
public:
	DL();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_DL_PRIMARY_RECORDTYPE;

	uint32 dlType() const;
	uint32 queryLimits() const;
};


//
// The CL Primary relation
//
class CL : public PrimaryRecord {
public:
	CL();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_CL_PRIMARY_RECORDTYPE;

	uint32 certTypeFormat() const;
	 uint32 certType() const { return certTypeFormat() >> 16; }
	 uint32 certEncoding() const { return certTypeFormat() & 0xFFFF; }
	uint32 crlTypeFormat() const;
	 uint32 crlType() const { return crlTypeFormat() >> 16; }
	 uint32 crlEncoding() const { return crlTypeFormat() & 0xFFFF; }
};


//
// The TP Primary relation
//
class TP : public PrimaryRecord {
public:
	TP();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_TP_PRIMARY_RECORDTYPE;

	uint32 certTypeFormat() const;
	 uint32 certType() const { return certTypeFormat() >> 16; }
	 uint32 certEncoding() const { return certTypeFormat() & 0xFFFF; }
};


//
// The TP Policy-OIDS relation
//
class PolicyOids : public Record {
public:
	PolicyOids();
	static const CSSM_DB_RECORDTYPE recordType = MDS_CDSADIR_TP_OIDS_RECORDTYPE;
	
	string moduleID() const;
	uint32 subserviceID() const;
	CssmData oid() const;
	CssmData value() const;
};


} // end namespace MDSClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_MDS_STANDARD
