// ======================================================================
//	File:		testKeychainAPI.h
//
//
//	Copyright:	Copyright (c) 2000,2003 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	4/18/00	em		Created.
// ======================================================================

#ifndef __TESTKEYCHAINAPI__
#define __TESTKEYCHAINAPI__

#include <stdio.h>
#include <Carbon/Carbon.h>
#undef check

class CTestApp{
public:
						CTestApp(
							int		inArgc,
							char **	inArgv);
						
	virtual void		Run();
					
	virtual	void 		DoRunScript(
							const char 		*inPath, 
							UInt32			&outPass, 
							UInt32			&outFail);
						
	virtual	void 		DoRunScript(
							FILE 			*inFile, 
							UInt32 			&outPass, 
							UInt32 			&outFail);
						
	virtual	void 		DoRunTestScript(
							const char 		*inScriptNo);
						
	virtual void		DoRunSubTestScript(
                                                        const char *		inScriptNo);
							
	virtual	void 		DoDumpScript(
							const char		*inOperationNo);
							
	virtual void		DoRadar(
							const char		*inBugNo);
	
	virtual	void		Cleanup();

	bool				IsVerbose(){ return mVerbose; }
	bool				IsRelaxErrorChecking(){ return mRelaxErrorChecking; }
protected:
	int 				mArgc;
	char **				mArgv;
	bool				mVerbose;
	bool				mRelaxErrorChecking;

};

#if TARGET_RT_MAC_MACHO
	#define	UNIX_fgets(a, b, c)	fgets(a, b, c)
#else
	#define UNIX_fgets(a, b, c)	UNIXfgets(a, b, c)
	char*	UNIX_fgets(char *inBuffer, int inSize, FILE *inFile);
#endif
		
#endif
