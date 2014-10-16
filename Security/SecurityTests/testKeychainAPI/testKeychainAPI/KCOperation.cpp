// ======================================================================
//	File:		KCOperation.cpp
//
//	pure virtual base class for performing operations in KeychainLib
//  (based on Dave Akhond's Operation for CDSA
//
//	Copyright:	Copyright (c) 2000,2003 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	3/28/00	em		Created.
// ======================================================================

#include "KCOperation.h"

#include <Security/cssmerr.h>

vector<KCRef>			KCOperation::sKCRefList;
vector<KCItemRef>		KCItemOperation::sKCItemRefList;
vector<KCSearchRef>		KCSearchOperation::sKCSearchRefList;
vector<AliasHandle>		KCOperation::sAliasList;

//vector<SecKeychainRef>		SecOperation::sSecRefList;
//vector<AliasHandle>		SecOperation::sAliasList;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ SetClient
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void				
Operation::SetClient(void *inClient)
{
	if(inClient == NULL) return;
	CTestApp	*aTestApp = static_cast<CTestApp*>(inClient);
	if(aTestApp == NULL) return;
	mClient = aTestApp;
}
								

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ ReadArguments
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void				
Operation::ReadArguments(
    FILE	*inFile)
{
    
    tParamList::iterator		aIterator = mParamList.begin();
    CParam *					aParam = *aIterator;
    
    while(aIterator != mParamList.end()){
        
        aParam->Read(inFile);
        aParam = *(++aIterator);
    }
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ WriteArguments
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void				
Operation::WriteArguments(
    FILE	*inFile)
{
    tParamList::iterator		aIterator = mParamList.begin();
    CParam *					aParam = *aIterator;
    while(aIterator != mParamList.end()){
        aParam->Write(inFile);
        aParam = *(++aIterator);
    }	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ WriteResults
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
Operation::WriteResults(
    FILE	*inFile)
{
	if (CSSM_BASE_ERROR <= mStatus && mStatus < CSSM_AC_BASE_ERROR + CSSM_ERRORCODE_MODULE_EXTENT)
		//fprintf(inFile, "     OSStatus CSSMERR_%s (0x%08lx)\n", cssmErrorString(mStatus).c_str(), mStatus);
                printf("Error");
	else
		fprintf(inFile, "     OSStatus %ld\n", mStatus);


    tParamList::iterator		aIterator = mResultList.begin();
    CParam *					aParam = *aIterator;
    while(aIterator != mResultList.end()){
        aParam->Write(inFile);
        aParam = *(++aIterator);
    }	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ ReadScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
int
Operation::ReadScript(
	FILE 			*inFile, 
    eKCOperationID	&outID)
{
    UInt32	aID;
    char	aBuffer[1024];
    char	aName[256];
    int		aResult;
   
    while(UNIX_fgets(aBuffer, sizeof(aBuffer)-1, inFile)){	
									// Read off comment lines
		if(aBuffer[0] == '/' && aBuffer[1] == '/') continue;
									// instructional comments
		if(aBuffer[0] == '#' && aBuffer[0] == '#'){
			for(UInt16 i=0; i<strlen(aBuffer)+4; i++) fprintf(stdout, "-");
			fprintf(stdout, "\n%s", aBuffer);
			for(UInt16 i=0; i<strlen(aBuffer)+4; i++) fprintf(stdout, "-");
			fprintf(stdout, "\n");
			continue;
		}
		
    	aResult = sscanf(aBuffer, "%ld %s\n", &aID, aName);
    	outID = (eKCOperationID)aID;
    	return aResult;
    }

    return(EOF);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ ReadScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
Operation::ReadScript(
	FILE 	*inFile)
{
    UInt32	aNumArgs = 0;
    UInt32	aSize = 0;
    fscanf(inFile, "   Input Arguments : %ld\n", &aNumArgs);
    
    ReadArguments(inFile);
     
    fscanf(inFile, "   Results : %ld\n", &aSize);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ WriteScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
Operation::WriteScript(
	FILE 	*inFile)
{
    fprintf(inFile, "%d %s\n", GetID(), COpRegister::GetOperationName(GetID()));
    fprintf(inFile, "   Input Arguments : %ld\n", mParamList.size());
    WriteArguments(inFile);
										// Count OSStatus as one of the result values
    fprintf(inFile, "   Results : %ld\n", mResultList.size()+1);
    WriteResults(inFile);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ GenerateScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
Operation::GenerateScript(
	FILE 	*inFile)
{
                                    // By default, just generate one script
    WriteScript(inFile);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ RunScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
bool
Operation::RunScript(
	FILE 	*inFile)
{
    ReadScript(inFile);
    Operate();
    return CompareResults(inFile);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ CompareResults
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
bool
Operation::CompareResults(
	FILE 	*inFile)
{
    OSStatus	aStatus;
    fscanf(inFile, "     OSStatus %ld\n", &aStatus);

	bool		aResult = true;
										// if gRelaxErrorChecking is on, don't bother
										// checking the actual error code, just check
										// for noErr-ness. 
	if(aStatus != mStatus){
		if(!mClient->IsRelaxErrorChecking()) aResult = false;
		if(mClient->IsRelaxErrorChecking() && ((aStatus == noErr) || (mStatus == noErr))) aResult = false;
	}
	
    tParamList::iterator		aIterator = mResultList.begin();
    CParam *					aParam = *aIterator;
    while(aIterator != mResultList.end()){
        if(aParam->Compare(inFile) == false)
            aResult = false;
        aParam = *(++aIterator);
    }
    return aResult;
}	

