// ======================================================================
//	File:		KCOperationID.h
//
//	OperationID must be registered so that a test script knows how to
//	construct an appropriate KCOperation class
//
//	Copyright:	Copyright (c) 2000,2003 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/22/00	em		Created.
// ======================================================================
#ifndef __KC_OPERATION_ID__
#define __KC_OPERATION_ID__
#include <stdio.h>
enum eKCOperationID {
	OpID_Unknown = -1,
	OpID_COp_KCGetKeychainManagerVersion = 0,
	OpID_COp_KeychainManagerAvailable,//1
	OpID_COp_KCMakeKCRefFromFSRef,//2
	OpID_COp_KCMakeKCRefFromFSSpec,//3
	OpID_COp_KCMakeKCRefFromAlias,//4
	OpID_COp_KCMakeAliasFromKCRef,//5
	OpID_COp_KCReleaseKeychain,//6
	OpID_COp_KCUnlockNoUI,//7
	OpID_COp_KCUnlock,//8
	OpID_COp_KCUnlockWithInfo,//9
	OpID_COp_KCLock,//10
	OpID_COp_KCLockNoUI,//11
	OpID_COp_KCGetDefaultKeychain,//12
	OpID_COp_KCSetDefaultKeychain,//13
	OpID_COp_KCCreateKeychain,//14
	OpID_COp_KCCreateKeychainNoUI,//15
	OpID_COp_KCGetStatus,//16
	OpID_COp_KCChangeSettingsNoUI,//17
	OpID_COp_KCGetKeychain,//18
	OpID_COp_KCGetKeychainName,//19
	OpID_COp_KCChangeSettings,//20
	OpID_COp_KCCountKeychains,//21
	OpID_COp_KCGetIndKeychain,//22
	OpID_COp_KCAddCallback,//23
	OpID_COp_KCRemoveCallback,//24
	OpID_COp_KCSetInteractionAllowed,//25
	OpID_COp_KCIsInteractionAllowed,//26
	OpID_COp_KCAddAppleSharePassword,//27
	OpID_COp_KCFindAppleSharePassword,//28
	OpID_COp_KCAddInternetPassword,//29
	OpID_COp_KCAddInternetPasswordWithPath,//30
	OpID_COp_KCFindInternetPassword,//31
	OpID_COp_KCFindInternetPasswordWithPath,//32
	OpID_COp_KCAddGenericPassword,//33
	OpID_COp_KCFindGenericPassword,//34
	OpID_COp_KCNewItem,//35
	OpID_COp_KCSetAttribute,//36
	OpID_COp_KCGetAttribute,//37
	OpID_COp_KCSetData,//38
	OpID_COp_KCGetData,//39
	OpID_COp_KCGetDataNoUI,//40
	OpID_COp_KCAddItem,//41
	OpID_COp_KCAddItemNoUI,//42
	OpID_COp_KCDeleteItem,//43
	OpID_COp_KCDeleteItemNoUI,//44
	OpID_COp_KCUpdateItem,//45
	OpID_COp_KCReleaseItem,//46
	OpID_COp_KCCopyItem,//47
	OpID_COp_KCFindFirstItem,//48
	OpID_COp_KCFindNextItem,//49
	OpID_COp_KCReleaseSearch,//50
	OpID_COp_KCFindX509Certificates,//51
	OpID_COp_KCChooseCertificate,//52
	OpID_COp_kcunlock,//53
	OpID_COp_kccreatekeychain,//54
	OpID_COp_kcgetkeychainname,//55
	OpID_COp_kcaddapplesharepassword,//56
	OpID_COp_kcfindapplesharepassword,//57
	OpID_COp_kcaddinternetpassword,//58
	OpID_COp_kcaddinternetpasswordwithpath,//59
	OpID_COp_kcfindinternetpassword,//60
	OpID_COp_kcfindinternetpasswordwithpath,//61
	OpID_COp_kcaddgenericpassword,//62
	OpID_COp_kcfindgenericpassword,//63
	OpID_COp_KCLogin,//64
	OpID_COp_KCLogout,//65
	OpID_COp_KCChangeLoginPassword,//66
	OpID_NumOperations
};
#define	IS_VALID_OPERATIONID(aID)	(aID >= 0 && aID < OpID_NumOperations)
	
typedef	void *	(*tClassCreatorFunc)(void *inClient);
typedef struct tOperationInfo{
	const char *		name;
	tClassCreatorFunc	func;
} tOperationInfo;
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COpRegister
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COpRegister {
public:
	static void *				CreateOperation(
		 							eKCOperationID			inID,
									void					*inClient)
		 						{
		 							RegisterAll();
                                    if(IS_VALID_OPERATIONID(inID)){
                                        if(sOperationInfoTbl[inID].func)
                                            return (sOperationInfoTbl[inID].func)(inClient);	
									}
                                    return NULL;
								}
	
	static const char *			GetOperationName(
		 							eKCOperationID			inID)
		 						{
		 							RegisterAll();
                                    if(IS_VALID_OPERATIONID(inID))
                                        return sOperationInfoTbl[inID].name;
                                    else
                                        return "INVALID OPERATION ID";
		 						}	
		 							
	static void					RegisterAll();
	static void					RegisterOne(
		 							eKCOperationID			inID,
		 							const char *			inName,
		 							tClassCreatorFunc		inFunc = NULL)
		 						{
		 							sOperationInfoTbl[inID].name = inName;
		 							sOperationInfoTbl[inID].func = inFunc;
		 						}
protected:
	static bool					sRegistered;
	static tOperationInfo		sOperationInfoTbl[OpID_NumOperations];
};
template <class T>
class TOpRegister{
public:
	static T *					Create(
									void	*inClient)
								{
									T	*aOperation = new T;
									if(aOperation) aOperation->SetClient(inClient);
									return aOperation;
								}
	static void					RegisterOne(
		 							eKCOperationID			inID,
		 							const char *			inName)
		 						{
		 							COpRegister::RegisterOne(inID, inName, (tClassCreatorFunc)Create);
		 						}	
};
										
										// Relax notations so one can avoid typing long names
#define KC_CLASS(funcname)				COp_## funcname
#define KC_OP_ID(funcname)				OpID_COp_## funcname
#define Register(funcname)				TOpRegister<KC_CLASS(funcname)>::RegisterOne(KC_OP_ID(funcname), (const char*)#funcname "")
#define OPERATION_ID(funcname)			enum{ operation_ID = KC_OP_ID(funcname) };\
										eKCOperationID	GetID(){ return (eKCOperationID)operation_ID; };
										
#endif	// __KC_OPERATION_ID__
