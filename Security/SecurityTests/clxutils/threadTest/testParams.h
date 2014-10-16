/*
 * Test parameters shared by all tests in threadTest suite.
 */
 
#ifndef	_TEST_PARAMS_H_
#define _TEST_PARAMS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Security/cssmtype.h>

typedef struct {
	unsigned		numLoops;
	char			verbose;
	char			quiet;
	unsigned		threadNum;
	unsigned		testNum;
	char			progressChar;
	CSSM_CSP_HANDLE	cspHand;
	CSSM_CL_HANDLE	clHand;
	CSSM_TP_HANDLE	tpHand;
	char			*testOpts;		// test-specific command line options
	void			*perThread;		// test-specific per-thread info, e.g., 
									// an array of CSSM_KEYs
} TestParams;

/* individual tests and their per-thread init routines */
extern int cgConstructInit(TestParams *testParams);
extern int cgConstruct(TestParams *testParams);
extern int cgVerifyInit(TestParams *testParams);
extern int cgVerify(TestParams *testParams);
extern int sslPingInit(TestParams *testParams);
extern int sslPing(TestParams *testParams);
extern int getFieldsInit(TestParams *testParams);
extern int getFields(TestParams *testParams);
extern int getCachedFieldsInit(TestParams *testParams);
extern int getCachedFields(TestParams *testParams);
extern int timeInit(TestParams *testParams);
extern int timeThread(TestParams *testParams);
extern int signVerifyInit(TestParams *testParams);
extern int signVerify(TestParams *testParams);
extern int symTestInit(TestParams *testParams);
extern int symTest(TestParams *testParams);
extern int attachTestInit(TestParams *testParams);
extern int attachTest(TestParams *testParams);
extern int rsaSignInit(TestParams *testParams);
extern int rsaSignTest(TestParams *testParams);
extern int desInit(TestParams *testParams);
extern int desTest(TestParams *testParams);
extern int sslThrashInit(TestParams *testParams);
extern int sslThrash(TestParams *testParams);
extern int cspRandInit(TestParams *testParams);
extern int cspRand(TestParams *testParams);
extern int derDecodeInit(TestParams *testParams);
extern int derDecodeTest(TestParams *testParams);
extern int secTrustEvalInit(TestParams *testParams);
extern int secTrustEval(TestParams *testParams);
extern int kcStatusInit(TestParams *testParams);
extern int kcStatus(TestParams *testParams);
extern int digestClientInit(TestParams *testParams);
extern int digestClient(TestParams *testParams);
extern int mdsLookupInit(TestParams *testParams);
extern int mdsLookup(TestParams *testParams);
extern int cssmErrStrInit(TestParams *testParams);
extern int cssmErrStr(TestParams *testParams);
extern int trustSettingsInit(TestParams *testParams);
extern int trustSettingsEval(TestParams *testParams);
extern int dbOpenCloseInit(TestParams *testParams);
extern int dbOpenCloseEval(TestParams *testParams);
extern int copyRootsInit(TestParams *testParams);
extern int copyRootsTest(TestParams *testParams);

/* etc. */

/* common thread-safe routines in threadTest.cpp */
CSSM_RETURN threadGetRandData(
	const TestParams 	*testParams,
	CSSM_DATA_PTR		data,		// mallocd by caller
	unsigned			numBytes);	// how much to fill
void randomDelay();
void printChar(
	char 				c);
	
#ifdef __cplusplus
}
#endif

#endif	/* _TEST_PARAMS_H_ */
