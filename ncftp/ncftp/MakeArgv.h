/* MakeArgv.h */

#ifndef _makeargv_h_
#define _makeargv_h_

/* Result codes returned after parsing a command line. */
#define kMavNoErr					0
#define kMavErrUnbalancedQuotes		1
#define kMavErrTooManyQuotePairs	2
#define kMavErrTooManyReDirections	3
#define kMavErrNoReDirectedFileName	4
#define kMavErrNoPipeCommand		5
#define kMavErrNoReDirectedInput	6
#define kMavErrBothPipeAndReDirOut	7

/* The different sets of quotes supported. */
#define ISQUOTE(c) (((c) == '"') || ((c) == '\'') || ((c) == '`'))

/* Max number of sets of nested quotes supported. */
#define kMaxQuotePairs 15

/* Special token types. */
#define kRegularArg 0
#define kReDirectOutArg 1
#define kReDirectOutAppendArg 2

/* Limit on number of arguments to track. */
#define kMaxArgs 128


typedef struct CmdLineInfo {
	int argCount;
	int isAppend;
	char outFileName[1024];
	char pipeCmdLine[1024];
	int savedStdout;
	int outFile;
	char argBuf[1024];
	char *argVector[kMaxArgs + 4];
	int err;
	char *errStr;
} CmdLineInfo, *CmdLineInfoPtr;

#define NEWCMDLINEINFOPTR \
	((CmdLineInfoPtr) calloc((size_t)1, sizeof(CmdLineInfo)))

/* We put a few more things in the arg vector. */
#define CMDLINEFROMARGS(c,v) (v[(c) + 1])
#define CLIPFROMARGS(c,v) ((CmdLineInfoPtr) (v[(c) + 2]))

int MakeArgVector(char *str, CmdLineInfoPtr clp);
void ExpandDollarVariables(char *cmdLine, size_t sz, int argc, char **argv);

#endif	/* _makeargv_h_ */

/* eof makeargv.h */
