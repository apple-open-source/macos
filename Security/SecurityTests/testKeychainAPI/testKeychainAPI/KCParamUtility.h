// ======================================================================
//	File:		KCParamUtility.h
//
//	Wrapper classes for function parameters
//	(using Parry's PodWrapper)
//
//
//	Copyright:	Copyright (c) 2000,2003,2006 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/22/00	em		Created.
// ======================================================================
#ifndef __KC_PARAM_UTILITY_
#define __KC_PARAM_UTILITY_

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
        //#include <SecurityCore/SecKeychainAPI.h> 
#else
	#include <Keychain.h>
#endif
#include <security_utilities/utilities.h>

#include <list>
#include <stdio.h>
#include <string>
#include <Carbon/Carbon.h>

#undef check
//typedef const char* SecStringPtr;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ CParam - base class for TParam template
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CParam
{
public:
	virtual void		Write(FILE *inFile)=0;
	virtual void		Read(FILE *inFile)=0;
	virtual bool 		Compare(FILE *inFile)=0;
};
typedef std::list<CParam*>	tParamList;
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ TParam
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
template <class Wrapper, class StructT, class T>
class TParam : public CParam
{
public:
                        TParam(const char *inName)
							:mName(inName), mResult(true)
						{
						}
						
                        TParam(const char *inName, T&inData)
							:mPod(inData), mName(inName), mResult(true)
						{
						}
						
	virtual				~TParam(){}
    
	bool				operator == (const T inData) const { return (mPod == inData); }
	Wrapper&			operator = (const T inData){ return mPod = inData; }
	Wrapper&			operator = (const T* inData){ return mPod = inData; }
						operator T*(){ return (T*)mPod; }
						operator T(){ return (T)mPod; }
	
	virtual void		Write(FILE *inFile)
						{ 
							WriteTitle(inFile);
							mPod.WriteData(inFile);
						}
						
	virtual void		Read(FILE *inFile) 
						{ 
							ReadTitle(inFile);
							mPod.ReadData(inFile);
						}
						
	virtual bool		Compare(FILE *inFile)
						{
							ReadTitle(inFile);
                            Wrapper	aWrapper;
                            aWrapper.ReadData(inFile);
                            return (mResult = (mPod == aWrapper));
                        }

protected:
	Wrapper				mPod;
	const char			*mName;
	bool				mResult;

	virtual void		WriteTitle(FILE *inFile)
						{
							if(mResult)
								fprintf(inFile, "     %s : ", mName); 
							else
								fprintf(inFile, "***  %s : ", mName);
						}
						
	virtual void		ReadTitle(FILE *inFile)
						{
                            char	aTitle[256];
                            fscanf(inFile, "%s : ", aTitle);
							if(::strcmp(mName, aTitle) != 0){
								throw(mName);
							}
						}

};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Non-struct POD wrapping macros
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
#define	POD_STRUCT(type)				type##Struct
#define POD_CLASS(type)					CParam##type
#define TYPEDEF_POD_STRUCT(type)		typedef struct type##Struct { type data; } type##Struct;
#define	TYPEDEF_POD_CLASS(type)			typedef TParam<C##type, type##Struct, type> CParam##type;
#define	TYPEDEF_STRUCTPOD_CLASS(type)	typedef TParam<C##type, type, type> CParam##type;
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ UInt32 wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(UInt32)
class CUInt32 :  public PodWrapper<CUInt32, POD_STRUCT(UInt32)>{
public:
                        CUInt32(){ data = 0; };
                        CUInt32(const UInt32 &inData){ data = inData; }
					
    CUInt32 &			operator = (const UInt32 &inData){ data = inData; return *this; }
	bool 				operator == (const UInt32 inData) const { return data == inData; }
						operator UInt32*(){ return &data; }
						operator UInt32(){ return data; }
						
	virtual void		WriteData(FILE *inFile)
                        {
                            fprintf(inFile, "%ld\n", data);
                        }
						
	virtual void		ReadData(FILE *inFile)
                        {
                            fscanf(inFile, "%ld\n", &data);
                        }
};	
TYPEDEF_POD_CLASS(UInt32)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ UInt16 wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(UInt16)
class CUInt16 :  public PodWrapper<CUInt16, POD_STRUCT(UInt16)>{
public:
                        CUInt16(){ data = 0; };
                        CUInt16(const UInt16 &inData){ data = inData; }
					
    CUInt16 &			operator = (const UInt16 &inData){ data = inData; return *this; }
	bool 				operator == (const UInt16 inData) const { return data == inData; }
                        operator UInt16*(){ return &data; }
                        operator UInt16(){ return data; }
						
	virtual void		WriteData(FILE *inFile)
                        {
                            fprintf(inFile, "%ld\n", (UInt32)data);
                        }
						
	virtual void		ReadData(FILE *inFile)
                        {
                            UInt32	aData;
                            fscanf(inFile, "%ld\n", &aData);
                            data = aData;
                        }
};	
TYPEDEF_POD_CLASS(UInt16)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Boolean wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(Boolean)
class CBoolean :  public PodWrapper<CBoolean, POD_STRUCT(Boolean)>{
public:
                        CBoolean(){ data = false; }
                        CBoolean(const Boolean &inData){ data = inData; }
                        operator Boolean*(){ return &data; }
                        operator Boolean(){ return data; }
						
    CBoolean &			operator = (const Boolean &inData){ data = inData; return *this; }
	bool 				operator == (const Boolean inData) const { return data == inData; }

	virtual void		WriteData(FILE *inFile)
                        { 
                            fprintf(inFile, "%d\n", data);
                        }
						
	virtual void		ReadData(FILE *inFile)
                        { 
                            int		aValue;
                            fscanf(inFile, "%d\n", &aValue);
                            data = ((aValue == 0) ? false : true);
                        }
						
};	
TYPEDEF_POD_CLASS(Boolean)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ FourCharCode wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(FourCharCode)
class CFourCharCode :  public PodWrapper<CFourCharCode, POD_STRUCT(FourCharCode)>{
public:
                        CFourCharCode(){ data = '????'; }
                        CFourCharCode(const FourCharCode &inData){ data = inData; }
					
    CFourCharCode &		operator = (const FourCharCode &inData){ data = inData; return *this; }
	bool 				operator == (const FourCharCode inData) const { return data == inData; }
                        operator FourCharCode*(){ return &data; }
                        operator FourCharCode(){ return data; }
						
	virtual void		WriteData(FILE *inFile)
                        { 
							for(UInt16 i=0; i<sizeof(FourCharCode); i++){
								fprintf(inFile, "%c", (char)(data >> ((sizeof(FourCharCode)-i-1) * 8)));
							}
							fprintf(inFile, "\n");
                        }
						
	virtual void		ReadData(FILE *inFile)
                        { 
                            FourCharCode	aValue = 0;
							for(UInt16 i=0; i<sizeof(FourCharCode); i++){
								char	aChar;
								fscanf(inFile, "%c", &aChar);
								aValue += (UInt32)aChar << ((sizeof(FourCharCode)-i-1)*8);
							}
							fscanf(inFile, "\n");
							data = aValue;
                        }
						
};	
TYPEDEF_POD_CLASS(FourCharCode)

typedef CParamFourCharCode	CParamOSType;
typedef CParamFourCharCode	CParamKCItemClass;
typedef CParamFourCharCode	CParamKCAttrType;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ AliasHandle wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(AliasHandle)
class CAliasHandle : public PodWrapper<CAliasHandle, POD_STRUCT(AliasHandle)>{
public:
                        CAliasHandle(){ data = NULL; }
                        CAliasHandle(const AliasHandle &inData){ data = inData; }
					
    CAliasHandle &		operator = (const AliasHandle &inData){ data = inData; return *this; }
	bool 				operator == (const AliasHandle inData) const { return data == inData; }
                        operator AliasHandle*(){ return &data; }
                        operator AliasHandle(){ return data; }
						
	virtual void		WriteData(FILE *inFile)
                        { 
							fprintf(inFile, "%s\n", mFullPathName);
                        }
						
	virtual void		ReadData(FILE *inFile)
                        { 
							memset(mFullPathName, 0, sizeof(mFullPathName));
							if(UNIX_fgets(mFullPathName, sizeof(mFullPathName), inFile)){
										// fgets grabs the newline code too
								mFullPathName[strlen(mFullPathName)-1] = 0;
							}
							else throw("Syntax error in CAliasHandle");
							
							if(strchr(mFullPathName, ':')){
										// Create a alias from the full-path name
						//%%%cpm - this WONT work, Keychain mgr does not fill in the FSSpec
								::NewAliasMinimalFromFullPath(
											strlen(mFullPathName),
											mFullPathName,
											NULL,
											NULL,
											&data);
							}
							else{
										// Ask KeychainLib to fill in the FSSpec for us
								FSSpec	tmpSpec = {0,0};
								tmpSpec.name[0] = ::strlen(mFullPathName);
								memcpy(tmpSpec.name+1, mFullPathName, tmpSpec.name[0]);
								
								KCRef	aKeychain;
								::KCMakeKCRefFromFSSpec(&tmpSpec, &aKeychain);
								::KCReleaseKeychain(&aKeychain);
						//%%%cpm - this WONT work, Keychain mgr does not fill in the FSSpec
                        		::NewAliasMinimal(
											&tmpSpec,
											&data);
							}
                        }
protected:
    char				mFullPathName[1024];
};	
TYPEDEF_POD_CLASS(AliasHandle)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ StringPtr wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(StringPtr)
class CStringPtr : public PodWrapper<CStringPtr, POD_STRUCT(StringPtr)>{
public:
                        CStringPtr(){ data = new unsigned char[256]; memset(data, 0, 256); }
                        CStringPtr(const StringPtr &inData){ memset(data, 0, 256); memcpy(data, inData, inData[0]); }
    virtual		~CStringPtr(){ delete data; }
                    
    CStringPtr &	operator = (const StringPtr inData){ memset(data, 0, 256); memcpy(data, inData, inData[0]); return *this; }
    bool 		operator == (const StringPtr inData) const { return ((data[0] == inData[0]) && (memcmp(data, inData, data[0]+1) == 0)); }
                        operator StringPtr*(){ return &data; }
                        operator StringPtr(){ return data; }
						
    virtual void	WriteData(FILE *inFile)
                        {
                            fprintf(inFile, "%s\n", data+1);
                        }
						
    virtual void	ReadData(FILE *inFile)
                        { 
                            memset(data, 0, 256);
							
                            char	cString[256];
							if(UNIX_fgets(cString, 256, inFile)){
								data[0] = strlen(cString)-1;
								memcpy(data+1, cString, data[0]);
							}
							else
								throw("Syntax error in CStringPtr");
                        }
};	
TYPEDEF_POD_CLASS(StringPtr)


TYPEDEF_POD_STRUCT(AFPServerSignature)
class CAFPServerSignature : public PodWrapper<CAFPServerSignature, POD_STRUCT(AFPServerSignature)>{
public:
                        CAFPServerSignature(){ memset(data, 0, sizeof(data)); }
                        CAFPServerSignature(const AFPServerSignature &inData){  memcpy(data, inData, sizeof(data)); }

    CAFPServerSignature &operator = (const AFPServerSignature inData){ memcpy(data, inData, sizeof(data)); return *this; }
	bool 				operator == (const AFPServerSignature inData) const { return (memcmp(data, inData, sizeof(data)) == 0); }
                        operator AFPServerSignature*(){ return &data; }
						
	virtual void		WriteData(FILE *inFile)
                        {
                            for(UInt16 i=0; i<sizeof(data); i++) fprintf(inFile, "%c", data[i]);
                            fprintf(inFile, "\n");
                        }
						
	virtual void		ReadData(FILE *inFile)
                        { 
                            for(UInt16 i=0; i<sizeof(data); i++) fscanf(inFile, "%c", (UInt8*)(data+i));
                            fscanf(inFile, "\n");
                        }
};
//TYPEDEF_POD_CLASS(AFPServerSignature)
typedef TParam<CAFPServerSignature, AFPServerSignatureStruct, AFPServerSignatureStruct> CParamAFPServerSignature;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥  Blob wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
typedef  struct kcBlob{UInt32 length; void* data; } kcBlob;
class CkcBlob : public PodWrapper<CkcBlob, kcBlob>{
public:
                        CkcBlob(){ length = 0; data = NULL; }
                        CkcBlob(const kcBlob &inData){ length = 0; data = NULL; DeepCopy(inData.length, inData.data); }
    CkcBlob &			operator = (const kcBlob &inData){ DeepCopy(inData.length, inData.data); return *this; }
	bool 				operator == (const kcBlob inData) const { return ((inData.length == length) && (memcmp(data, inData.data, length) == 0)); }
                        operator kcBlob*(){ return this; }

#if defined(__MWERKS__)
                        operator kcBlob(){ return *this; }
#endif                        
                                            
	virtual void		WriteData(FILE *inFile)
                        {
                            fprintf(inFile, "/%ld/", length);
							if(length > 0){
								for(UInt32 i=0; i<length; i++) fprintf(inFile, "%c", ((UInt8*)data)[i]);
							}
							fprintf(inFile, "\n");
                        }
						
	virtual void		ReadData(FILE *inFile)
                        {
							UInt32	aLength;
                            fscanf(inFile, "/%ld/", &aLength);

										// recyle 'data' if the size remains the same
							if(aLength != length){
								if(data) delete (UInt8*)data;
								data = NULL;
								if(aLength > 0) data = (UInt8*)new char[aLength+2];
								length = aLength;
							}
							if(length > 0){
								UNIX_fgets((char*)data, aLength+3, inFile);
								((UInt8*)data)[length] = 0;
							}
							else
								fscanf(inFile, "\n");
                        }
protected:
    virtual void		DeepCopy(UInt32 inLength, const void *inData)
                        {
                            if(data != NULL) delete (UInt8*)data;
                            data = NULL; 
                            
                            length = inLength; 
                            if(length == 0) return;
                            data = (UInt8*)new char[length];
                            memcpy(data, inData, length);
                        }
};
TYPEDEF_STRUCTPOD_CLASS(kcBlob)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ FSSpec wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CFSSpec : public PodWrapper<CFSSpec, FSSpec>{
public:
                        CFSSpec(){ vRefNum = 0; parID = 0; memset(name, 0, sizeof(name)); mFullPathName[0] = 0;}
                        CFSSpec(const FSSpec &inData){ memcpy(this, &inData, sizeof(*this)); }
    CFSSpec &			operator = (const FSSpec &inData){ memcpy(this, &inData, sizeof(*this)); return *this; }
	bool 				operator == (const FSSpec inData) const { return (this == &inData) || !memcmp(this, &inData, sizeof(FSSpec)); }
                        operator FSSpec*(){ return this ; }
                    
	virtual void		WriteData(FILE *inFile)
                        {
							fprintf(inFile, "%s\n", mFullPathName);
                        }
						
	virtual void		ReadData(FILE *inFile)
                        {
							memset(mFullPathName, 0, sizeof(mFullPathName));
							if(UNIX_fgets(mFullPathName, sizeof(mFullPathName), inFile)){
										// fgets grabs the newline code too
								name[0] = strlen(mFullPathName)-1;
								mFullPathName[name[0]] = 0;
								memcpy(name+1, mFullPathName, name[0]);
							}
							else throw("Syntax error in CFSSpec");
                        }
protected:
    char				mFullPathName[1024];
};
TYPEDEF_STRUCTPOD_CLASS(FSSpec)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ FSRef wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CFSRef : public PodWrapper<CFSRef, FSRef>{
public:
                        CFSRef(){ memset(hidden, 0, sizeof(hidden)); }
                        CFSRef(const FSRef &inData){ memcpy(this, &inData, sizeof(*this)); }
					
    CFSRef &			operator = (const FSRef &inData){ memcpy(this, &inData, sizeof(*this)); return *this; }
	bool 				operator == (const FSRef inData) const  { return (this == &inData) || !memcmp(this, &inData, sizeof(FSRef)); }
                        operator FSRef*(){ return this; }
						
	virtual void		WriteData(FILE *inFile)
                        {
                                            // ¥¥¥ need work
                            fprintf(inFile, "\n");
                        }
					
	virtual void		ReadData(FILE *inFile)
                        { 
                                            // ¥¥¥ need work
                            fscanf(inFile, "\n");
                        }	
};
TYPEDEF_STRUCTPOD_CLASS(FSRef)


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCAttribute wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CKCAttribute : public PodWrapper<CKCAttribute, KCAttribute>{
public:
                        CKCAttribute(){ tag = '0000'; length = 0; data = NULL; }
                        CKCAttribute(const KCAttribute &inData){ memcpy(this, &inData, sizeof(*this)); }
					
    CKCAttribute &		operator = (const KCAttribute &inData){ memcpy(this, &inData, sizeof(*this)); return *this; }
	bool 				operator == (const KCAttribute inData) const
						{ 
							if(inData.tag != tag) return false;
							return(memcmp(inData.data, data, ((inData.length < length) ? inData.length : length)) == 0);
						}
							
                        operator KCAttribute*(){ return this; }
#if defined(__MWERKS__)
						operator KCAttribute(){ return *this; }
#endif												
	virtual void		WriteData(FILE *inFile)
                        {
							fprintf(inFile, "\n");
							
							CParamKCAttrType	aTag(".tag", tag);
							aTag.Write(inFile);

							kcBlob				theBlob = {length, data};
							CParamkcBlob		aData(".data", theBlob);
							aData.Write(inFile);
                        }
					
	virtual void		ReadData(FILE *inFile)
                        { 
                            fscanf(inFile, "\n");
							
							CParamKCAttrType	aTag(".tag");
							aTag.Read(inFile);
							tag = aTag;
							
							CParamkcBlob		aData(".data");
							aData.Read(inFile);
							
							kcBlob				aBlob;
							aBlob = aData;
							length = aBlob.length;
							data = aBlob.data;
                        }	
};
TYPEDEF_STRUCTPOD_CLASS(KCAttribute)


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCAttributeList wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CKCAttributeList : public PodWrapper<CKCAttributeList, KCAttributeList>{
public:
                        CKCAttributeList(){ count = 0; attr = NULL; }
                        CKCAttributeList(const CKCAttributeList &inData){ memcpy(this, &inData, sizeof(*this)); }
					
    CKCAttributeList &	operator = (const CKCAttributeList &inData){ memcpy(this, &inData, sizeof(*this)); return *this; }
	bool 				operator == (const CKCAttributeList inData) const  { return (this == &inData) || !memcmp(this, &inData, sizeof(CKCAttributeList)); }
                        operator CKCAttributeList*(){ return this; }
						
	virtual void		WriteData(FILE *inFile)
                        {
							fprintf(inFile, "\n");
							
							CParamUInt32	aCount(".count", count);
							aCount.Write(inFile);
							
							for(UInt32 i=0; i<count; i++){
								char aAttributeTitle[32];
								sprintf(aAttributeTitle, ".%ld", i);
								CParamKCAttribute	aAttr(aAttributeTitle, *(attr+i));
								aAttr.Write(inFile);
							}
                        }
					
	virtual void		ReadData(FILE *inFile)
                        { 
                            fscanf(inFile, "\n");
							
							CParamUInt32	aCount(".count");
							aCount.Read(inFile);

										// recycle if the size does not change
							if(count != (UInt32)aCount){
								if(attr) delete attr;
								count = (UInt32)aCount;
								attr = new KCAttribute[count];
							}
							
							for(UInt32 i=0; i<count; i++){
								char aAttributeTitle[32];
								sprintf(aAttributeTitle, ".%ld", i);
								CParamKCAttribute	aAttr(aAttributeTitle);
								aAttr.Read(inFile);
								*(attr+i) = (KCAttribute)aAttr;
							}
                        }	
};
TYPEDEF_STRUCTPOD_CLASS(KCAttributeList)

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ CKCRef wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(KCRef)
class CKCRef : public PodWrapper<CKCRef, POD_STRUCT(KCRef)>{
public:
						CKCRef(){ data = NULL; }
                        CKCRef(const KCRef &inData){ data = inData; }
	bool				operator == (const KCRef inKC)
						{
							if(inKC == data) return true;
								
							char	thisName[256] = "";
							char	aInName[256] = "";
							::kcgetkeychainname(data, thisName);
							::kcgetkeychainname(inKC, aInName);
							return(::strcmp(thisName, aInName));
						}	
};


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCItemRef wrapper
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
TYPEDEF_POD_STRUCT(KCItemRef)
class CKCItemRef : public PodWrapper<CKCItemRef, POD_STRUCT(KCItemRef)>{
public:
						CKCItemRef(){ data = NULL; }
                        CKCItemRef(const KCItemRef &inData){ data = inData; }
	bool				operator == (const KCItemRef inKCItem)
						{
							if(inKCItem == data) return true;
	
							KCRef	thisKeychain = 0;
							KCRef	aInKeychain = 0;
							::KCGetKeychain(data, &thisKeychain);
							::KCGetKeychain(inKCItem, &aInKeychain);
							if((CKCRef(thisKeychain) == aInKeychain) == false) return false;
	
// Bug #2458217 - (KCGetData() causes bus error)
#if TARGET_RT_MAC_MACHO
	return true;
#else
							UInt32	thisLength;
							UInt32	aInLength;
							::KCGetData(data, 0, NULL, &thisLength);
							::KCGetData(inKCItem, 0, NULL, &aInLength);
							if(thisLength != aInLength) return false;
	
							char	*thisData = new char[thisLength];
							char	*aInData = new char[aInLength];
							::KCGetData(data, thisLength, thisData, &thisLength);
							::KCGetData(inKCItem, aInLength, aInData, &aInLength);
							
							int aResult = ::memcmp(thisData, aInData, thisLength);
							
							delete thisData;
							delete aInData;
							return(aResult == 0);
#endif
						}
};


#endif	// __KC_PARAM_UTILITY_
