#include "KCAPI_Keychain.h"
#include "KCAPI_Manager.h"
#include "testKeychainAPI.h"
#include "Radar.h"
#include <unistd.h>

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
#else
	#include <Keychain.h>
	#include <iostream>
	#include <SIOUX.h>
	static void GetArg(int &outArgc, char**	&outArgv);
#endif

static char*	gResourcePath = NULL;
static char*	GetResourcePath(char**argv);
static int gSleep=0;

#ifndef TEST_SCRIPT_PATH
	#define TEST_SCRIPT_PATH	(getenv("TESTKEYCHAINAPI_TEST_SCRIPT_PATH") ? getenv("TESTKEYCHAINAPI_TEST_SCRIPT_PATH") : gResourcePath)
#endif


int main(int argc, char** argv)
{

#if defined(__MWERKS__)
										// Set SIOUX window position to top left corner
										// so that Security Dialogs will not cover the
										// window
	SIOUXSettings.toppixel = 40;
	SIOUXSettings.leftpixel = 5;

										// emulate argc, argv
	GetArg(argc, argv);
#endif
	
	gResourcePath = GetResourcePath(argv);
	
	try{
		CTestApp	aTestApp(argc, argv);
					aTestApp.Run();
	}
	catch(const char *inErrorMsg){
		fprintf(stderr, "ERROR : %s\n", inErrorMsg);
	}
	
	if(gSleep)
	{
		fprintf(stderr, "\n-----> sleeping...\n");
		sleep(60000);
	}
	
}

CTestApp::CTestApp(
	int 	inArgc,
	char **	inArgv)
	:mArgc(inArgc), mArgv(inArgv),
	mVerbose(false), mRelaxErrorChecking(false)
{	
}

void
CTestApp::Run()
{
	int				ch;
	bool 			didWork = false;
	const char		*options = "hH?vlew:f:r:R:n:s:S";
	
#if TARGET_RT_MAC_MACHO
	extern char *	optarg;
	while ((ch = getopt(mArgc, mArgv, options)) != -1){
#else
	char *	optarg = NULL;
	for(int i=1; i<mArgc; i++){
		if(mArgv[i][0] == '-'){
			char *p;
			ch = mArgv[i][1];
			if(p = strchr(options, ch))
				if(p[1] == ':') optarg = mArgv[++i];
		}		
#endif
		switch(ch){
			case 'v':{
				mVerbose = true;
				break;
			}

			case 'e':{
				mRelaxErrorChecking = true;
				break;
			}
			
			case 's':{
				DoRunSubTestScript(optarg);       
				didWork = true;
				break;
			}
			
			case 'w':{
				DoDumpScript(optarg);
				didWork = true;
				break;
			}	

			case 'f':{
				UInt32	aPass, aFail;
				DoRunScript(optarg, aPass, aFail);
				didWork = true;
				break;
			}
			
			case 'n':{
				DoRunTestScript(optarg);       
				didWork = true;
				break;
			}
			
			case 'R':
			case 'r':{
				DoRadar(optarg);
				didWork = true;
				break;
			}
			
			case 'l':{
				DoRadar(NULL);
				didWork = true;
				break;
			}
			
			case 'h':
			case 'H':
			case '?':
				break;
				
			case 'S':
				gSleep=1;
				break;
		}
	}
	if(!didWork) goto showUsage;
        return;
showUsage:
    {
        char	* aProgname = strrchr(mArgv[0], '/');
        aProgname = (aProgname) ? aProgname+1 : mArgv[0];
    
        fprintf(stderr, "Usage : %s [-v][-e][-r {all} {ID}][-l][-n scriptNo][-s scriptNo][-f filename][-w {all} {ID}][-S]  \n", aProgname);
        fprintf(stderr, "        -v    verbose\n");
        fprintf(stderr, "        -e    relax return code checking\n");
        fprintf(stderr, "        -s    run sub-test cases\n");
        fprintf(stderr, "        -r    run a Radar bug\n");
        fprintf(stderr, "        -l    list all Radar bugs\n");
        fprintf(stderr, "        -n    run a test script\n");
        fprintf(stderr, "        -f    run your script\n");
        fprintf(stderr, "        -w    dump a script template\n");
        fprintf(stderr, "        -S    sleep before exiting (for use with MallocDebug)\n");
        fprintf(stderr, "\n      default args -venall\n");


    }
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoRunTestScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
CTestApp::DoRunTestScript(
	const char *inScriptNo)
{
	FILE	*aInput;
	char	aFullPath[256];
	UInt32	aPassCount = 0;
	UInt32	aFailCount = 0;
	
        if(!strcmp(inScriptNo, "all")){
		for(UInt32 i=0; i<50 /* 9999 */; i++){
#if TARGET_RT_MAC_MACHO
			sprintf(aFullPath, "%s/script%04ld", TEST_SCRIPT_PATH, i);
#else
			sprintf(aFullPath, "%s:script%04ld", TEST_SCRIPT_PATH, i);
#endif
			if((aInput = fopen(aFullPath, "r")) != NULL){
				UInt32	aPass=0, aFail=0;
				if(mVerbose) printf("\n\n-> -> -> Running script%04ld ...\n", i);
				DoRunScript(aInput, aPass, aFail);
				aPassCount += aPass;
				aFailCount += aFail;
				fclose(aInput);
			}
		}
	}
        else{
	
#if TARGET_RT_MAC_MACHO
		sprintf(aFullPath, "%s//script%04d", TEST_SCRIPT_PATH, atoi(inScriptNo));
#else
		sprintf(aFullPath, "%s:script%04d", TEST_SCRIPT_PATH, atoi(inScriptNo));
#endif
		
		if((aInput = fopen(aFullPath, "r")) != NULL){
			DoRunScript(aInput, aPassCount, aFailCount);
			fclose(aInput);
		}
		else{
			fprintf(stderr, "No script file for [%s]\n", inScriptNo);
			return;
		}
	}

	printf("Total number of test cases executed %ld : (passed = %ld, failed = %ld)\n", aPassCount+aFailCount, aPassCount, aFailCount);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoRunSubTestScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
CTestApp::DoRunSubTestScript(
	const char *	inScriptNo)
{
	FILE	*aInput;
	char	aFullPath[256];
	UInt32	aPassCount = 0;
	UInt32	aFailCount = 0;
	UInt32	aStartScript = 0;
	UInt32	aEndScript = 50; /* 9999 */
        
                                        // not "all" - run the specified test case's sub-test cases
        if(strcmp(inScriptNo, "all")){
            aStartScript = aEndScript = atoi(inScriptNo);
        }
        
        for(UInt32 j=aStartScript; j<=aEndScript; j++){
            for(UInt32 i=0; i<5 /*999*/; i++){
#if TARGET_RT_MAC_MACHO
		sprintf(aFullPath, "%s//script%04ld.%03ld", TEST_SCRIPT_PATH, j, i);
#else
		sprintf(aFullPath, "%s:script%04ld.%03ld", TEST_SCRIPT_PATH, j, i);
#endif

		if((aInput = fopen(aFullPath, "r")) != NULL){
			UInt32	aPass, aFail;
			if(mVerbose) printf("\n\n-> -> -> Running script%04ld.%03ld ...\n", j, i);
			DoRunScript(aInput, aPass, aFail);
			aPassCount += aPass;
			aFailCount += aFail;
			fclose(aInput);
		}
            }
	}
	
	printf("Total number of test cases executed %ld : (passed = %ld, failed = %ld)\n", aPassCount+aFailCount, aPassCount, aFailCount);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoRunScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void 
CTestApp::DoRunScript(
	const char	*inPath,
	UInt32		&outPass,
	UInt32		&outFail)
{
	Cleanup();
	
	FILE			*aInput = fopen(inPath, "r");
	if(aInput == NULL){
		fprintf(stderr, "ERROR Cannot open the file %s\n", inPath);
		return;
	}
	DoRunScript(aInput, outPass, outFail);
	fclose(aInput);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoRunScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void 
CTestApp::DoRunScript(
	FILE	*inFile,
	UInt32	&outPass,
	UInt32	&outFail)
{
	Cleanup();

	eKCOperationID	aID;
	UInt32			aPassCount = 0;
	UInt32			aFailCount = 0;
	
	try{
		while(KCOperation::ReadScript(inFile, aID) != EOF){
                                       KCOperation	*aOperation = (KCOperation*)COpRegister::CreateOperation(aID, this);
			if(aOperation == NULL){
				fprintf(stderr, "ERROR COpRegister::CreateOperation(%d) failed\n", aID);
				break;
			}

			bool	aResult = aOperation->RunScript(inFile);
			aPassCount += (aResult) ? 1 : 0;
			aFailCount += (!aResult) ? 1 : 0;
			if(mVerbose){
				printf("TestCase No.%04ld (%s): %s\n", aPassCount+aFailCount, COpRegister::GetOperationName(aID), (aResult) ? "PASSED" : "FAILED");
				if(!aResult){
					fprintf(stdout, "%d %s\n", aID, COpRegister::GetOperationName(aID));
					aOperation->WriteResults(stdout);
				}
			}
			delete aOperation;
		}
	}
	catch(const char *inErrorMsg){
		fprintf(stderr, "ERROR : %s\n", inErrorMsg);
		fprintf(stderr, "   Terminating this script\n");
	}
	
	outPass = aPassCount;
	outFail = aFailCount;

}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoDumpScript
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void 
CTestApp::DoDumpScript(
	const char	*inOperationNo)
{
	FILE	*aOutput = stdout;
	bool	aDoAll = (strcmp(inOperationNo, "all") == 0);
	
	for(long i=0; i<OpID_NumOperations; i++){
		if(aDoAll || (atoi(inOperationNo) == i)){
			KCOperation	*aOperation = (KCOperation*)COpRegister::CreateOperation((eKCOperationID)i, this);
			if(aOperation == NULL){
				fprintf(stderr, "ERROR COpRegister::CreateOperation(%ld) failed\n", i);
				continue;
			}
//			aOperation->Operate();
			aOperation->WriteScript(aOutput);
			delete aOperation;
		}
	}
}



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ DoRadar
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void 
CTestApp::DoRadar(
	const char *inArg)
{
    UInt32	i=0;

										// -l (list all Radar bug descriptions)
    if(inArg == NULL){
        while(gRadarBugs[i].testCaseFunc){
            printf("%s %s\n", gRadarBugs[i].ID, gRadarBugs[i].desc);
            i++;
        }
    }
										// -r all (run all Radar bugs)
    else if(!strcmp(inArg, "all")){
        while(gRadarBugs[i].testCaseFunc){
            printf(">>>>>>>>>> Radar %s >>>>>>>>>>\n", gRadarBugs[i].ID);
            printf("%s\n", gRadarBugs[i].desc);
            (gRadarBugs[i].testCaseFunc)(this);
            printf("<<<<<<<<<< Radar %s done <<<<<<<<<<\n\n", gRadarBugs[i].ID);

            i++;
        }
    }
										// -r nnnnn (run a specific Radar bug)
    else{
        while(gRadarBugs[i].testCaseFunc){
            if(!strcmp(inArg, gRadarBugs[i].ID)){
                (gRadarBugs[i].testCaseFunc)(this);
                return;
            }
            i++;
        }
        fprintf(stderr, "No such Radar ID\n");
        DoRadar(NULL);
    }
}

void
CTestApp::Cleanup()
{
	KCOperation::Cleanup();
	KCItemOperation::Cleanup();
	KCSearchOperation::Cleanup();
}

#if TARGET_RT_MAC_MACHO
char*
GetResourcePath(char **inArgv)
{
									// Can I do better than this ?
	const char	*aResourcesDir = "../Resources/";
	const char	*progname;
	char		*aResourcePath = NULL;
	int			len;
	
	progname = strrchr(inArgv[0], '/');
	len = (progname) ? (strlen(inArgv[0]) - strlen(progname)+1) : 0;
	aResourcePath = new char[len + strlen(aResourcesDir)+1];
	if(len) memcpy(aResourcePath, inArgv[0], len);
	memcpy(aResourcePath+len, aResourcesDir, strlen(aResourcesDir));
        aResourcePath[len+strlen(aResourcesDir)] = '\0';
	return(aResourcePath);
}
#else
char*
GetResourcePath(char **inArgv)
{
										// ¥¥¥ temp solution
	return("Work:CVS Project:Security:Tests:testKeychainAPI:testKeychainAPI:scripts");
}

void GetArg(
	int		&outArgc,
	char**	&outArgv)
{
										// emulate the command line arg
	const char	*kProgname = "testKeychainAPI";
	char		*aBuffer = new char[1024];
	
	memset(aBuffer, 0, 1024);
	strcpy(aBuffer, kProgname);
	int	i = strlen(kProgname)+1;

	cout << kProgname << " ";
	cin.getline(aBuffer+i, 1024-i);
	for(; i<1028; i++) if(aBuffer[i] == ' ') aBuffer[i] = '\0';

	int		j = 0;
	char	*p = aBuffer;
	char	**argv = new char*[128];
	do{
		if(strlen(p) > 0) argv[j++] = p; 
		p += strlen(p)+1;
	} while(p < aBuffer+1024);

	outArgc = j;
	outArgv = argv;
}

char*
UNIX_fgets(
	char	*inBuffer,
	int		inSize,
	FILE	*inFile)
{
									// Scripts are in UNIX format
									// so fgets() does not work on ClassicMac
									// as is
	for(int i=0; i<inSize; i++){
		inBuffer[i] = fgetc(inFile);
		if(inBuffer[i] != '\r' && inBuffer[i] != '\n' && inBuffer[i] != EOF) continue;
		if(inBuffer[i] == EOF && i==0) return NULL;
		inBuffer[i] = '\n';
		inBuffer[i+1] = '\0';
		break;
	}
	return inBuffer;
}

#endif
