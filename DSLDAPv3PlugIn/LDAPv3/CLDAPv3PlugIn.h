/*
	File:		CLDAPv3PlugIn.h

	Contains:	LDAP v3 plugin definitions to work with LDAP v3 Plugin implementation
			that interfaces with Directory Services

	Copyright:	© 2001 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/


#ifndef __CLDAPv3PlugIn_h__
#define __CLDAPv3PlugIn_h__	1

#include <stdio.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CDataBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>
#include <DirectoryServiceCore/CDSServerModule.h>

#include "CLDAPNode.h"
#include "CLDAPv3Configs.h"	//used to read the XML config data for each user defined LDAP server

#include <LDAP/lber.h>
#include <LDAP/ldap.h>

const uInt32	kBufferTax	= 16;
#define MaxDefaultLDAPNodeCount 4

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
	kAuthNativeMethod		= 138,
    kAuthSetPasswdAsRoot 	= 139
};


typedef struct {
    char	*fName;
    bool	fAvail;
    int		fPort;
} sLDAPNode;

typedef struct {
    int				msgId;				//LDAP session call handle mainly used for searches
    LDAPMessage	   *pResult;			//LDAP message last result used for continued searches
    uInt32			fRecNameIndex;		//index used to cycle through all requested Rec Names
    uInt32			fRecTypeIndex;		//index used to cycle through all requested Rec Types
    uInt32			fTotalRecCount;		//count of all retrieved records
    uInt32			fLimitRecSearch;	//client specified limit of number of records to return
    void			*fAuthHndl;
	void			*fAuthHandlerProc;
	char			*fAuthAuthorityData;
    tContextData	fPassPlugContinueData;
} sLDAPContinueData;

typedef sInt32 (*AuthAuthorityHandlerProc) (tDirNodeReference inNodeRef,
											tDataNodePtr inAuthMethod,
											sLDAPContextData* inContext,
											sLDAPContinueData** inOutContinueData,
											tDataBufferPtr inAuthData,
											tDataBufferPtr outAuthData,
											bool inAuthOnly,
											char* inAuthAuthorityData,
                                            CLDAPv3Configs *inConfigFromXML,
                                            CLDAPNode& inLDAPSessionMgr );

class CLDAPv3PlugIn : public CDSServerModule
{
public:
						CLDAPv3PlugIn				(	void );
	virtual			   ~CLDAPv3PlugIn				(	void );

	virtual sInt32		Validate					(	const char *inVersionStr,
														const uInt32 inSignature );
	virtual sInt32		Initialize					(	void );
	virtual sInt32		ProcessRequest				(	void *inData );
	virtual sInt32		SetPluginState				(	const uInt32 inState );
	static	void		ContextDeallocProc			(	void* inContextData );
	static	void		ContinueDeallocProc			(	void *inContinueData );

	static sInt32		DoBasicAuth					(	tDirNodeReference inNodeRef,
                                                        tDataNodePtr inAuthMethod, 
                                                        sLDAPContextData* inContext, 
                                                        sLDAPContinueData** inOutContinueData, 
                                                        tDataBufferPtr inAuthData, 
                                                        tDataBufferPtr outAuthData, 
                                                        bool inAuthOnly,
                                                        char* inAuthAuthorityData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
                                                        
	static sInt32		DoPasswordServerAuth		(	tDirNodeReference inNodeRef,
                                                        tDataNodePtr inAuthMethod, 
                                                        sLDAPContextData* inContext, 
                                                        sLDAPContinueData** inOutContinueData, 
                                                        tDataBufferPtr inAuthData, 
                                                        tDataBufferPtr outAuthData, 
                                                        bool inAuthOnly,
                                                        char* inAuthAuthorityData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );	
    

protected:
	void				WakeUpRequests				(	void );
	void				WaitForInit					(	void );
	sInt32				HandleRequest				(	void *inData );
    sInt32				OpenDirNode					(	sOpenDirNode *inData );
    sInt32				CloseDirNode				(	sCloseDirNode *inData );
	sInt32				GetDirNodeInfo				(	sGetDirNodeInfo *inData );
    sInt32				GetRecordList				(	sGetRecordList *inData );
    sInt32				GetAllRecords				(	char *inRecType,
														char *inNativeRecType,
														CAttributeList *inAttrTypeList,
														sLDAPContextData *inContext,
														sLDAPContinueData *inContinue,
														bool inAttrOnly,
														CBuff *inBuff,
														uInt32 &outRecCount,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
														ber_int_t inScope );
	sInt32				GetTheseRecords				(	char *inConstRecName,
														char *inConstRecType,
														char *inNativeRecType,
														tDirPatternMatch patternMatch,
														CAttributeList *inAttrTypeList,
														sLDAPContextData *inContext,
														sLDAPContinueData *inContinue,
														bool inAttrOnly,
														CBuff *inBuff, uInt32 &outRecCount,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
														ber_int_t inScope );
    char			   *GetRecordName 				(	char *inRecType,
														LDAPMessage	*inResult,
														sLDAPContextData *inContext,
														sInt32 &errResult );
    sInt32				GetRecordEntry				(	sGetRecordEntry *inData );
	sInt32				GetTheseAttributes			(	char *inRecType,
														CAttributeList *inAttrTypeList,
														LDAPMessage *inResult,
														bool inAttrOnly,
														sLDAPContextData *inContext,
														sInt32 &outCount,
														CDataBuff *inDataBuff );
	sInt32				GetAttributeEntry			(	sGetAttributeEntry *inData );
	sInt32				GetAttributeValue			(	sGetAttributeValue *inData );
	static char			*MapAttrToLDAPType			(	char *inRecType,
														char *inAttrType,
														uInt32 inConfigTableIndex,
														int inIndex,
                                                        CLDAPv3Configs *inConfigFromXML,
														bool bSkipLiteralMappings = false );
	char			  **MapAttrListToLDAPTypeArray	(	char *inRecType,
														CAttributeList *inAttrTypeList,
														uInt32 inConfigTableIndex );
	static char		  **MapAttrToLDAPTypeArray		(	char *inRecType,
														char *inAttrType,
														uInt32 inConfigTableIndex,
                                                        CLDAPv3Configs *inConfigFromXML );
	static char		   *MapRecToLDAPType			(	char *inRecType,
														uInt32 inConfigTableIndex,
														int inIndex,
														bool *outOCGroup,
														CFArrayRef *outOCListCFArray,
														ber_int_t *outScope,
                                                        CLDAPv3Configs *inConfigFromXML );
	uInt32				CalcCRC						(	char *inStr );
    static sInt32		CleanContextData			(	sLDAPContextData *inContext );
    sLDAPContextData   *MakeContextData				(	void );
    void				PrintNodeName 				(	tDataListPtr inNodeList );
	static char		   *BuildLDAPQueryFilter		(	char *inConstAttrType,
														char *inConstAttrName,
														tDirPatternMatch patternMatch,
														uInt32 inConfigTableIndex,
														bool useWellKnownRecType,
														char *inRecType,
														char *inNativeRecType,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
                                                        CLDAPv3Configs *inConfigFromXML );
	sInt32				OpenRecord					(	sOpenRecord *inData );
	sInt32				CloseRecord					(	sCloseRecord *inData );
	sInt32				FlushRecord					(	sFlushRecord *inData );
	sInt32				DeleteRecord				(	sDeleteRecord *inData );
	sInt32				CreateRecord				(	sCreateRecord *inData );
	sInt32				AddAttribute				(	sAddAttribute *inData );
	sInt32				AddAttributeValue			(	sAddAttributeValue *inData );
	sInt32				AddValue					(	uInt32 inRecRef,
														tDataNodePtr inAttrType,
														tDataNodePtr inAttrValue );
	sInt32				RemoveAttribute				(	sRemoveAttribute *inData );
	sInt32				RemoveAttributeValue		(	sRemoveAttributeValue *inData );
	sInt32				SetAttributeValue			(	sSetAttributeValue *inData );
	sInt32				SetRecordName				(	sSetRecordName *inData );
	sInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
	sInt32				CloseAttributeList			(	sCloseAttributeList *inData );
	sInt32				CloseAttributeValueList		(	sCloseAttributeValueList *inData );
	sInt32				GetRecRefInfo				(	sGetRecRefInfo *inData );
	sInt32				GetRecAttribInfo			(	sGetRecAttribInfo *inData );
	sInt32				GetRecAttrValueByIndex		(	sGetRecordAttributeValueByIndex *inData );
	sInt32				GetRecordAttributeValueByID	(	sGetRecordAttributeValueByID *inData );
	char			   *GetNextStdAttrType			(	char *inRecType, uInt32 inConfigTableIndex, int &inputIndex );
	sInt32				DoAttributeValueSearch		(	sDoAttrValueSearchWithData *inData );
	bool				DoTheseAttributesMatch		(	sLDAPContextData *inContext,
														char *inAttrName,
														tDirPatternMatch	pattMatch,
														LDAPMessage		   *inResult);
	static bool			DoesThisMatch				(	const char		   *inString,
														const char		   *inPatt,
														tDirPatternMatch	inPattMatch );
	sInt32				FindAllRecords              (	char *inConstAttrName,
                                                        char *inConstRecType,
                                                        char *inNativeRecType,
                                                        tDirPatternMatch patternMatch,
                                                        CAttributeList *inAttrTypeList,
                                                        sLDAPContextData *inContext,
                                                        sLDAPContinueData *inContinue,
                                                        bool inAttrOnly,
                                                        CBuff *inBuff,
                                                        uInt32 &outRecCount,
                                                        bool inbOCANDGroup,
                                                        CFArrayRef inOCSearchList,
														ber_int_t inScope );
	sInt32				FindTheseRecords			(	char *inConstAttrType,
                                                        char *inConstAttrName,
                                                        char *inConstRecType,
                                                        char *inNativeRecType,
                                                        tDirPatternMatch patternMatch,
                                                        CAttributeList *inAttrTypeList,
                                                        sLDAPContextData *inContext,
                                                        sLDAPContinueData *inContinue,
                                                        bool inAttrOnly,
                                                        CBuff *inBuff,
                                                        uInt32 &outRecCount,
                                                        bool inbOCANDGroup,
                                                        CFArrayRef inOCSearchList,
														ber_int_t inScope );
                                                        
	sInt32				DoAuthentication			(	sDoDirNodeAuth *inData );
    
    static sInt32		DoSetPassword	 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
                                                        
    static sInt32		DoSetPasswordAsRoot 		(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
                                                        
    static sInt32		DoChangePassword 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );

	static sInt32		GetAuthMethod				(	tDataNode *inData,
														uInt32 *outAuthMethod );
    
    static sInt32		RepackBufferForPWServer		(	tDataBufferPtr inBuff,
                                                        const char *inUserID,
                                                        unsigned long inUserIDNodeNum,
                                                        tDataBufferPtr *outBuff );
    
    static sInt32		PWOpenDirNode				(	tDirNodeReference fDSRef,
                                                        char *inNodeName,
                                                        tDirNodeReference *outNodeRef );

    sInt32 				GetAuthAuthority			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
                                                        unsigned long *outAuthCount,
                                                        char **outAuthAuthority[] );

    sInt32				ParseAuthAuthority			(	const char * inAuthAuthority,
                                                        char **outVersion,
                                                        char **outAuthTag,
                                                        char **outAuthData );

	static sInt32		DoUnixCryptAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
	static sInt32		DoClearTextAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
														bool authCheckOnly,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
	static char		   *GetDNForRecordName			(	char* inRecName,
														sLDAPContextData *inContext,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr );
	sInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );

	static sInt32		VerifyLDAPSession			(	sLDAPContextData *inContext,
														int inContinueMsgId,
                                                        CLDAPNode& inLDAPSessionMgr );

	static sInt32		RebindLDAPSession			(	sLDAPContextData *inContext, CLDAPNode& inLDAPSessionMgr );

	sInt32				GetRecRefLDAPMessage		(	sLDAPContextData *inRecContext,
														LDAPMessage **outResultMsg );
	bool				ParseNextDHCPLDAPServerString
													(	char **inServer,
														char **inSearchBase,
														int *inPortNumber,
														bool *inIsSSL,
														int inServerIndex );
    static CFMutableStringRef
                        ParseCompoundExpression		(	char *inConstAttrName,
														char *inRecType,
														uInt32 inConfigTableIndex,
                                                        CLDAPv3Configs *inConfigFromXML );
    
    static sInt32		GetUserNameFromAuthBuffer	(	tDataBufferPtr inAuthData,
                                                        unsigned long inUserNameIndex,
                                                        char **outUserName );
    
	AuthAuthorityHandlerProc GetAuthAuthorityHandler (	const char* inTag );

private:
	sMapTuple		   *pStdAttributeMapTuple;
	sMapTuple		   *pStdRecordMapTuple;
	bool				bCheckForDHCPLDAPServers;
	bool				bDoNotInitTwiceAtStartUp;	//don't want setpluginstate call to init again at startup
	CFStringRef			fDHCPLDAPServersString;
	uInt32				fState;
	uInt32				fSignature;
	CLDAPv3Configs	   *pConfigFromXML;
	CLDAPNode			fLDAPSessionMgr;
	uInt32				fDefaultLDAPNodeCount;		//count of LDAP server nodes obtained from DHCP
	char			   *pDefaultLDAPNodes[MaxDefaultLDAPNodeCount];		
													//actual node names of the LDAP server nodes

};

#endif	// __CLDAPv3PlugIn_h__
