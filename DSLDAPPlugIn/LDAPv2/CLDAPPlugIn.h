/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLDAPPlugIn
 * LDAP plugin definitions to work with LDAP Plugin implementation.
 */

#ifndef __CLDAPPlugIn_h__
#define __CLDAPPlugIn_h__	1

#include <stdio.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CDataBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>
#include <DirectoryServiceCore/CDSServerModule.h>

#include "CLDAPConfigs.h"	//used to read the XML config data for each user defined LDAP server

#include <LDAP/lber.h>
#include <LDAP/ldap.h>

const uInt32	kBufferTax	= 16;

enum eBuffType {
    kRecordListType		= 'RecL'
};

enum {
	kAuthUnknowMethod		= 127,
	kAuthClearText			= 128,
	kAuthCrypt				= 129,
	kAuthSetPasswd			= 130,
	kAuthChangePasswd		= 131,
	kAuthAPOP				= 132,
	kAuth2WayRandom			= 133,
	kAuthNativeClearTextOK	= 134,
	kAuthNativeNoClearText	= 135,
	kAuthSMB_NT_Key			= 136,
	kAuthSMB_LM_Key			= 137,
	kAuthNativeMethod		= 138
};


typedef struct {
    char	*fName;
    bool	fAvail;
    int		fPort;
} sLDAPNode;

// Context data structure
//KW need to get away from using UserDefName, Name, and Port in the context but get it
// from the config table
typedef struct sLDAPContextData {
	LDAP		   *fHost;				//LDAP session handle
	uInt32			fConfigTableIndex;	//gConfigTable Hash index
	char		   *fName;				//LDAP domain name ie. ldap.apple.com
	int				fPort;				//LDAP port number - default is 389
	int				fType;				//KW type of reference entry - not used yet
    int				msgId;				//LDAP session call handle mainly used for searches
    bool			authCallActive;		//indicates if authentication was made through the API
    									//call and if set means don't use config file auth name/password
    char		   *authAccountName;	//Account name used in auth
    char		   *authPassword;		//Password used in auth
    LDAPMessage	   *pResult;			//LDAP message last result
    uInt32			fRecNameIndex;		//index used to cycle through all requested Rec Names
    uInt32			fRecTypeIndex;		//index used to cycle through all requested Rec Types
    uInt32			fTotalRecCount;		//count of all retrieved records
    uInt32			fLimitRecSearch;	//client specified limit of number of records to return
    uInt32			fAttrIndex;			//index used to cycle through all requested Attrs
    uInt32			offset;				//offset into the data buffer
    uInt32			index;
    uInt32			attrCnt;
    char		   *fOpenRecordType;	//record type used to open a record
    char		   *fOpenRecordName;	//record name used to open a record
} sLDAPContextData;

class CLDAPPlugIn : public CDSServerModule
{
public:
                	CLDAPPlugIn		( void );
	virtual		   ~CLDAPPlugIn		( void );

	virtual sInt32	Validate		( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32	Initialize		( void );
	virtual sInt32	ProcessRequest	( void *inData );
	virtual sInt32	SetPluginState	( const uInt32 inState );
	static	void	ContextDeallocProc ( void* inContextData );

protected:
	void			WakeUpRequests		( void );
	void			WaitForInit			( void );
	sInt32			HandleRequest		( void *inData );
    sInt32			OpenDirNode			( sOpenDirNode *inData );
    sInt32			CloseDirNode		( sCloseDirNode *inData );
	sInt32			GetDirNodeInfo		( sGetDirNodeInfo *inData );
    sInt32			GetRecordList		( sGetRecordList *inData );
    sInt32			GetAllRecords		( char *inRecType, char *inNativeRecType, CAttributeList *inAttrTypeList,
                              				sLDAPContextData *inContext, bool inAttrOnly, CBuff *inBuff, uInt32 &outRecCount );
	sInt32			GetTheseRecords		( char *inConstRecName, char *inConstRecType, char *inNativeRecType,
											tDirPatternMatch patternMatch, CAttributeList *inAttrTypeList,
											sLDAPContextData *inContext, bool inAttrOnly, CBuff *inBuff, uInt32 &outRecCount );
    char		       *GetRecordName 		( LDAPMessage	*inResult, sLDAPContextData	*inContext, sInt32 &errResult );
    sInt32			GetRecordEntry		( sGetRecordEntry *inData );
    sInt32			GetRecInfo			( char *inData, tRecordEntryPtr *outRecInfo );
	sInt32			GetTheseAttributes	( CAttributeList *inAttrTypeList, LDAPMessage *inResult,
										  bool inAttrOnly, sLDAPContextData *inContext, sInt32 &outCount, CDataBuff *inDataBuff );
	sInt32			GetAttributeEntry	( sGetAttributeEntry *inData );
	sInt32			GetAttributeValue	( sGetAttributeValue *inData );
	char		   *MapAttrToLDAPType	( char *inAttrType, uInt32 inConfigTableIndex, int inIndex );
	char		  **MapAttrToLDAPTypeArray	( char *inAttrType, uInt32 inConfigTableIndex );
	char		   *MapRecToLDAPType	( char *inRecType, uInt32 inConfigTableIndex, int inIndex );
	uInt32			CalcCRC				( char *inStr );
    static sInt32	CleanContextData	( sLDAPContextData *inContext );
    sLDAPContextData   *MakeContextData		( void );
    void			PrintNodeName 		( tDataListPtr inNodeList );
	char		   *BuildLDAPQueryFilter( char *inConstAttrType, char *inConstAttrName,
											tDirPatternMatch patternMatch, int inConfigTableIndex,
											bool useWellKnownRecType );
	sInt32			OpenRecord			( sOpenRecord *inData );
	sInt32			CloseRecord			( sCloseRecord *inData );
	sInt32			CloseAttributeList	( sCloseAttributeList *inData );
	sInt32			CloseAttributeValueList
										( sCloseAttributeValueList *inData );
	sInt32			GetRecRefInfo		( sGetRecRefInfo *inData );
	sInt32			GetRecAttribInfo	( sGetRecAttribInfo *inData );
	sInt32			GetRecAttrValueByIndex
										( sGetRecordAttributeValueByIndex *inData );
	char		   *GetNextStdAttrType	( uInt32 inConfigTableIndex, int inIndex );
	sInt32			DoAttributeValueSearch
										( sDoAttrValueSearchWithData *inData );
	bool			DoTheseAttributesMatch
										(	sLDAPContextData	   *inContext,
											char			   *inAttrName,
											tDirPatternMatch	pattMatch,
											LDAPMessage		   *inResult);
	bool			DoesThisMatch		(	const char		   *inString,
											const char		   *inPatt,
											tDirPatternMatch	inPattMatch );
	sInt32			FindAllRecords		( char *inConstAttrName, char *inConstRecType,
											char *inNativeRecType, tDirPatternMatch	patternMatch,
											CAttributeList *inAttrTypeList, sLDAPContextData *inContext,
											bool inAttrOnly, CBuff *inBuff, uInt32 &outRecCount );
	sInt32			FindTheseRecords	( char *inConstAttrType, char *inConstAttrName,
											char *inConstRecType, char *inNativeRecType,
											tDirPatternMatch patternMatch, CAttributeList *inAttrTypeList,
											sLDAPContextData *inContext, bool inAttrOnly, CBuff *inBuff, uInt32 &outRecCount );
	sInt32			DoAuthentication	( sDoDirNodeAuth *inData );
	sInt32			GetAuthMethod		( tDataNode *inData, uInt32 *outAuthMethod );
	sInt32			DoUnixCryptAuth		( sLDAPContextData *inContext, tDataBuffer *inAuthData );
	sInt32			DoClearTextAuth		( sLDAPContextData *inContext, tDataBuffer *inAuthData, bool authCheckOnly );
	char		   *GetDNForRecordName	( char* inRecName, sLDAPContextData *inContext );
	sInt32			DoPlugInCustomCall	( sDoPlugInCustomCall *inData );
	sInt32			RebindTryProc		( sLDAPContextData *inContext );
	
private:
        sMapTuple	   *pStdAttributeMapTuple;
        sMapTuple	   *pStdRecordMapTuple;
        uInt32			fState;
        uInt32			fSignature;
        CLDAPConfigs   *pConfigFromXML;

};

#endif	// __CLDAPPlugIn_h__
