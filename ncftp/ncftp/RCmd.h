/* RCmd.h */

#ifndef _rcmd_h_
#define _rcmd_h_ 1

#ifndef _linelist_h_
#include "LineList.h"
#endif

typedef struct Response {
	int codeType;
	int code;
	int printMode;
	int eofOkay;
	int hadEof;
	LineList msg;
} Response, *ResponsePtr;

/* Optional field entry 'printMode' can be filled in with one of these.
 * PrintResponse looks at this to see if we will really print it.
 */
#define kDontPrint -1
#define kDoPrint 1
#define kDefaultPrint 0		/* We'll decide based on the response code then. */

#define kQuiet 0
#define kErrorsOnly 1
#define kTerse 2
#define kVerbose 3

/* Used for selective screening of certain responses. */
#define kAllRmtMsgs					0
#define kNoChdirMsgs				00001
#define kNoConnectMsg				00002

#define kDefaultResponse ((ResponsePtr) 0)
#define kIgnoreResponse ((ResponsePtr) -1)

#define CLEARRESPONSE(R)	PTRZERO(R, sizeof(Response))

#define kDefaultNetworkTimeout 30

/* Declared in xfer.h */
typedef struct XferSpec *XferSpecPtr;

#include "Open.h"

int SetVerbose(int newVerbose);
ResponsePtr InitResponse(void);
void PrintResponseIfNeeded(ResponsePtr rp);
void PrintResponse(ResponsePtr rp);
int GetTelnetString(char *str, size_t siz, FILE *cin, FILE *cout);
void DoneWithResponse(ResponsePtr rp);
void ReInitResponse(ResponsePtr rp);
int GetResponse(ResponsePtr rp);
void TraceResponse(ResponsePtr rp);

#ifdef HAVE_STDARG_H
int RCmd(ResponsePtr rp0, char *cmdspec0, ...);
int RDataCmd(XferSpecPtr xp0, char *cmdspec0, ...);
#else
int RCmd();
int RDataCmd();
#endif

#endif	/* _rcmd_h_ */
