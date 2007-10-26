/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __CCLENGINE__
#define __CCLENGINE__



/* ccl commands	*/
enum
{
	cNoCmd			= 0,
	cComment		= 1,
	cCCLScript		= 2,
	cOriginateLabel		= 3,
	cAnswerLabel		= 4,
	cHangUpLabel		= 5,
	cScriptLabel			= 6,
	cAsk			= 7,
	cChrDelay		= 8,
	cCommunicatingAt	= 9,
	cDecTries		= 10,
	cDTRSet			= 11,
	cDTRClear		= 12,
	cExit			= 13,
	cFlush			= 14,
	cHSReset		= 15,
	cIfAnswer		= 16,
	cIfOriginate		= 17,
	cIfStr			= 18,
	cIfTries		= 19,
	cIncTries		= 20,
	cJump			= 21,
	cJSR			= 22,
	cLBreak			= 23,
	cLogMsg			= 24,	// undocumented.  use cNote.  KW
	cMatchClr		= 25,
	cMatchRead		= 26,
	cMatchStr		= 27,
	cNote			= 28,
	cPause			= 29,
	cReturn			= 30,
	cSBreak			= 31,
	cSerReset		= 32,
	cSetSpeed		= 33,
	cSetTries		= 34,
	cUserHook		= 35,
	cWrite			= 36,
	cMonitorLine		= 37,
	cDebugLevel		= 38,

	cFirstCmd		= cComment,
	cLastCmd		= cDebugLevel
};

/* ccl varStrings	*/
enum				// for varIndex
{
	vsRangeStart		= 1,
	vsDialString		= 1,		// full dial string
	vsModemSpeaker		= 2,		// modem speaker flag
	vsTonePulse		= 3,		// tone/pulse dialing
	vsErrorCorrection	= 4,		// error correction
	vsDataCompression	= 5,		// data compression
	vsDialMode		= 6,		// dialing mode
	vsDialString1		= 7,		// first dial string fragment
	vsDialString2		= 8,		// second dial string fragment
	vsDialString3		= 9,		// third dial string fragment
	vsAsk			= 10,		// string returned from above
	vsUserName		= 11,		// username for chat script
	vsPassWord		= 12,		// password for chat script

	//Extended varString range (^2 doesn't appear in existing dial strings)
	vsConnectSpeed		= 20,		// for solving flakiness
	vsInit			= 21,		// initialization AT command

	//GPRS arguments
	vsAPN	    		= 22,		// Access Point Name
	vsCID   		= 23,		// Connection ID
	
	//Reserved (Custom Reset? Hangup? Dial?)
	vsReserved1		= 24,
	vsReserved2		= 25,
	vsReserved3		= 26,
	
	//undefined arguments passed from plist to script
	vsString27		= 27,
	vsString28		= 28,
	vsString29		= 29,
	vsString30		= 30,
		
	vsMax			= 30		// total number of var strings
};
/* parameter counts	*/
enum
{
	kHSResetParamCount	= 6,
	kIfStrParamCount	= 3,
	kIfTriesParamCount	= 2,
	kSerResetParamCount	= 4
};

/* miscellaneous constants	*/

#define MAXLABELS		128
// big maxMatch values slow matching when there are no matches
// some memory is also allocated, but not a Str255 ;)
#define maxMatch		96		// was 16 in 1.0 (MacOS 6?)
#define	cclNestingLimit		16

#define SHORTBREAK		0x000001F4	// 0.5 secs = 500 msecs = 0x000001F4
#define LONGBREAK		0x00000DAC	// 3.5 secs = 3500 msecs = 0x00000DAC

#define	DTR_SET			17		// control opcode for setting dtr
#define DTR_CLEAR		18		// control opcode for clearing dtr

#define kMaxRequeuedDataSize	64		// OT won't allocate anything less than 64

/* recognized types of modem compression - all other values are	*/
/* reserved for specific types of compression			*/

#define	cclCompress_None	0		// no compression
#define	cclCompress_Unspecified	1		// unspecified compression

/* recognized types of modem error correction protocol - all other values	*/
/* are reserved for specific protocols										*/

#define	cclProtocol_None	0		// no error correction Protocol
#define	cclProtocol_Unspecified	1		// unspecified error correction Protocol
#define	cclProtocol_MNP10	2		// MNP-10 error correction Protocol

#define ACTIVITY_LOG		1		// NOTE goes to Activity Log.
#define STATUS_WINDOW		2		// NOTE goes to Status Window.

/* ascii codes */
enum
{
	ENTER			= 0x03,		/* ascii code for enter				*/
	BACK_SPACE		= 0x08,		/* ascii code for back space			*/
	TABUL			= 0x09,		/* ascii code for tab				*/
	NL			= 0x0A,		/* ascii code for line feed			*/
	CR			= 0x0D,		/* ascii code for carriage return		*/
	PERIOD			= 0x2E,		/* ascii code for .				*/
	DOT			= 0xA5,		/* ascii code for €				*/
	LEFTARROW		= 0x1C,
	RIGHTARROW		= 0x1D,
	UPARROW			= 0x1E,
	DOWNARROW		= 0x1F,

	/* control characters	*/

	chrETX			= 0x03,
	chrBS			= 0x08,
	chrHT			= 0x09,
        chrNL			= 0x0A,
        chrCR			= 0x0D,
	chrFS			= 0x1C,
	chrGS			= 0x1D,
	chrRS			= 0x1E,
	chrUS			= 0x1F,
	chrSpace		= 0x20,
	chrDblQuote		= 0x22,
	chrQuote		= 0x27,
	chrComma		= 0x2C,
	chrSemiColon		= 0x3B,
	chrBackSlash		= 0x5C,
	chrCaret		= 0x5E,
	chrPassWord		= 0xA5
};


/* ccl control flags	*/
enum
{
	cclAsk			= 0x0001,	/* set when @ASK command is pending */
	cclPlaying		= 0x0002,	/* script is currently playing */
	cclWantAbort		= 0x0004,	/* available for recycling */
	cclMatchReadAbort	= 0x0008,	/* available for recycling */
	cclSysHeap		= 0x0010,	/* available for recycling */
	cclAskEnable		= 0x0020,	/* Set if the Ask command is enabled */
	cclReOriginateMode	= 0x0040,	/* set for reconnecting after disconnect */
	cclTimerPosted		= 0x0080,	/* available for recycling */
	cclDisabledfClientQ	= 0x0100,	/* set in DoDataIndication, reset in DoConnectResponse */
	cclBoundToAnswer	= 0x0200,	/* set so we can return to Answering after a connection */
	cclAnswerConnFailed	= 0x0400,	/* Set when a conn_req failed in Answer mode */
	cclAnswerMode		= 0x0800,	/* Set when we want to answer an incoming call */
	cclPrepScriptFailed	= 0x1000,	/* Set when pre-flight of a script fails */
	cclMatchPending		= 0x2000,	/* Set for a pending matchread */
	cclOriginateMode	= 0x4000,	/* Set when we want to originate an outgoing call */
	cclHangupMode		= 0x8000	/* Set when we want to hangup the modem */
};


#define MAX_SCRIPT_SIZE		32000	// max number of characters in the script
#define	MAX_SCRIPT_LINES	32000	// max number of lines in a script


typedef struct TRMatchStrInfo
{
	u_int8_t		*matchStr;		// pointer to the match string
	char		delimiterChar;		// string delimiter, " or '
        u_int8_t		matchStrIndex;		// index of next char in match string
	u_int8_t		*varStr;		// pointer to var string being compared
	u_int8_t		inVarStr;		// flag when matching var string
        u_int8_t		varStrIndex;		// index of next char in var string
	char		varStrSize;		// size of var string
	short		matchLine;		// line to jump to on match
} TRMatchStrInfo, *TPMatchStrInfo;



typedef struct TRScriptVars
{
	unsigned short	ctlFlags;		// CCL control flags
	u_int32_t	serialSpeed;		/* the last speed the serial driver was set to	*/
	char		maskStringId;		/* varString subject to bullet masking	*/
	unsigned char	maskStart;		/* starting mask character position		*/
	unsigned char	maskStop;		/* stopping mask character position		*/
	short		theAbortErr;		/* result code for the abort			*/
	unsigned char	modemReliability;	/* type of reliability negotiated by modem	*/
	unsigned char	modemCompression;	/* type of compression negotiated by modem	*/
	void		*commands;		// ptr to ccl commands
	short		answerLine;		// index to answer entry
	short		originateLine;		// index to originate entry
	short		hangUpLine;		// index to hangUp entry
	u_int32_t	pauseTimer;		// Value of the pause timer
	u_int32_t	chrDelayValue;		// character delay value
	u_int8_t	*script;		// ptr to CCL script
	u_int8_t	scriptPrepped;		// true if PrepScript has been called
	u_int8_t	scriptPrepFailed;	// true if PrepScript fails; used in Connect/Disconnect.
	u_int32_t	scriptAllocSize;	// byte size of allocation for CCL script
	u_int32_t	scriptSize;		// byte size of CCL script
	u_int16_t	lineCount;		// number of lines in the script
	u_int16_t	*indexTable;		// ptr to script line index table
	u_int16_t	scriptLineIndex;	// index into current script line
	u_int16_t	scriptLine;		// index to current script line
	u_int8_t	*scriptLinePtr;		// pointer to current script line
	u_int8_t	scriptLineSize;		// size, in bytes of current script line
	u_int32_t	loopCounter;		// just what you think it is
	short		labels[MAXLABELS];	// script line indices for labels
	TRMatchStrInfo	matchStr[ maxMatch];	// match string information for each match string
	u_int8_t	strBuf[256];		// buffer used for temorary string storage
	u_int16_t	askLabel;		// label to jump to if user cancels ask dialog
        ushort		stack[cclNestingLimit];	// stack used for subroutine jumps
        u_int32_t	topOfStack;		// index of top of stack
	u_int8_t	writeBufIndex;		// index into current write request
	u_int8_t	logMaskOn;		// tells whether to mask sensitive varString text when logging
} TRScriptVars, *TPScriptVars;


#endif	/* __CCLENGINE__ */
