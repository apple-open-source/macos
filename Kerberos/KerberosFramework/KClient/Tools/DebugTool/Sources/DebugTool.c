#include "DebugTool.h"

#include <stdio.h>

short IsDriverOpen(StringPtr driverName);
DCtlHandle FindTheDriver(StringPtr driverName);
void SetSelectorsDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selectors [4], Boolean state);

#define KCLIENTDRIVER "\p.Kerberos"

char selectorNames [][64] = {
/*   0 */	"¥¥¥Invalid selector¥¥¥",
/*   1 */	"cKrbKillIO",
/*   2 */	"cKrbGetLocalRealm",
/*   3 */	"cKrbSetLocalRealm",
/*   4 */	"cKrbGetRealm",
/*   5 */	"cKrbAddRealmMap",
/*   6 */	"cKrbDeleteRealmMap",
/*   7 */	"cKrbGetNthRealmMap",
/*   8 */	"cKrbGetNthServer",
/*   9 */	"cKrbAddServerMap",
/*  10 */	"cKrbDeleteServerMap",
/*  11 */	"cKrbGetNthServerMap",
/*  12 */	"cKrbGetNumSessions",
/*  13 */	"cKrbGetNthSession",
/*  14 */	"cKrbDeleteSession",
/*  15 */	"cKrbGetCredentials",
/*  16 */	"cKrbAddCredentials",
/*  17 */	"cKrbDeleteCredentials",
/*  18 */	"cKrbGetNumCredentials",
/*  19 */	"cKrbGetNthCredentials",
/*  20 */	"cKrbDeleteAllSessions",
/*  21 */	"cKrbGetTicketForService",
/*  22 */	"cKrbGetAuthForService",
/*  23 */	"cKrbCheckServiceResponse",
/*  24 */	"cKrbEncrypt",
/*  25 */	"cKrbDecrypt",
/*  26 */	"cKrbCacheInitialTicket",
/*  27 */	"cKrbGetUserName",
/*  28 */	"cKrbSetUserName",
/*  29 */	"cKrbSetPassword",
/*  30 */	"cKrbGetDesPointers",
/*  31 */	"cKrbGetErrorText",
/*  32 */	"cKrbLogin",
/*  33 */	"cKrbSetKey",
/*  34 */	"cKrbKerberos",
/*  35 */	"cKrbGetNthServerPort",
/*  36 */	"cKrbSetNthServerPort",
/*  37 */	"cKrbDriverVersion",
/*  38 */	"cKrbPasswordToKey",
/*  39 */	"cKrbNewClientSession",
/*  40 */	"cKrbNewServerSession",
/*  41 */	"cKrbDisposeSession",
/*  42 */	"cKrbServerVerifyTicket",
/*  43 */	"cKrbServerGetReplyTkt",
/*  44 */	"cKrbGetServiceKey",
/*  45 */	"cKrbAddServiceKey",
/*  46 */	"cKrbGetOption",
/*  47 */	"cKrbSetOption",
/*  48 */	"cKrbAdditionalLogin",
/*  49 */	"cKrbControlPanelEnter",
/*  50 */	"cKrbControlPanelLeave",
/*  51 */	"cKrbGetSessionTimeRemaining",
/*  52 */	"cKrbGetSessionUserName",
/*  53 */	"cKrbGetNumSpecials",
/*  54 */	"cKrbGetNthSpecial",
/*  55 */	"cKrbAddSpecial",
/*  56 */	"cKrbDeleteSpecial",
/*  57 */	"cKrbCheckUnencryptedServiceResponse",
/*  58 */	"cKrbVerifyIntegrity",
/*  59 */	"cKrbProtectIntegrity",
/*  60 */	"¥¥¥Invalid selector¥¥¥",
/*  61 */	"¥¥¥Invalid selector¥¥¥",
/*  62 */	"¥¥¥Invalid selector¥¥¥",
/*  63 */	"¥¥¥Invalid selector¥¥¥",
/*  64 */	"¥¥¥Invalid selector¥¥¥",
/*  65 */	"accRun (system tickle)",
/*  66 */	"¥¥¥Invalid selector¥¥¥",
/*  67 */	"¥¥¥Invalid selector¥¥¥",
/*  68 */	"¥¥¥Invalid selector¥¥¥",
/*  69 */	"¥¥¥Invalid selector¥¥¥",
/*  70 */	"¥¥¥Invalid selector¥¥¥",
/*  71 */	"¥¥¥Invalid selector¥¥¥",
/*  72 */	"¥¥¥Invalid selector¥¥¥",
/*  73 */	"¥¥¥Invalid selector¥¥¥",
/*  74 */	"¥¥¥Invalid selector¥¥¥",
/*  75 */	"¥¥¥Invalid selector¥¥¥",
/*  76 */	"¥¥¥Invalid selector¥¥¥",
/*  77 */	"¥¥¥Invalid selector¥¥¥",
/*  78 */	"¥¥¥Invalid selector¥¥¥",
/*  79 */	"¥¥¥Invalid selector¥¥¥",
/*  80 */	"¥¥¥Invalid selector¥¥¥",
/*  81 */	"¥¥¥Invalid selector¥¥¥",
/*  82 */	"¥¥¥Invalid selector¥¥¥",
/*  83 */	"¥¥¥Invalid selector¥¥¥",
/*  84 */	"¥¥¥Invalid selector¥¥¥",
/*  85 */	"¥¥¥Invalid selector¥¥¥",
/*  86 */	"¥¥¥Invalid selector¥¥¥",
/*  87 */	"¥¥¥Invalid selector¥¥¥",
/*  88 */	"¥¥¥Invalid selector¥¥¥",
/*  89 */	"¥¥¥Invalid selector¥¥¥",
/*  90 */	"¥¥¥Invalid selector¥¥¥",
/*  91 */	"¥¥¥Invalid selector¥¥¥",
/*  92 */	"¥¥¥Invalid selector¥¥¥",
/*  93 */	"¥¥¥Invalid selector¥¥¥",
/*  94 */	"¥¥¥Invalid selector¥¥¥",
/*  95 */	"¥¥¥Invalid selector¥¥¥",
/*  96 */	"¥¥¥Invalid selector¥¥¥",
/*  97 */	"¥¥¥Invalid selector¥¥¥",
/*  98 */	"¥¥¥Invalid selector¥¥¥",
/*  99 */	"¥¥¥Invalid selector¥¥¥",
/* 100 */	"¥¥¥Invalid selector¥¥¥",
/* 101 */	"¥¥¥Invalid selector¥¥¥",
/* 102 */	"¥¥¥Invalid selector¥¥¥",
/* 103 */	"¥¥¥Invalid selector¥¥¥",
/* 104 */	"¥¥¥Invalid selector¥¥¥",
/* 105 */	"¥¥¥Invalid selector¥¥¥",
/* 106 */	"¥¥¥Invalid selector¥¥¥",
/* 107 */	"¥¥¥Invalid selector¥¥¥",
/* 108 */	"¥¥¥Invalid selector¥¥¥",
/* 109 */	"¥¥¥Invalid selector¥¥¥",
/* 110 */	"¥¥¥Invalid selector¥¥¥",
/* 111 */	"¥¥¥Invalid selector¥¥¥",
/* 112 */	"¥¥¥Invalid selector¥¥¥",
/* 113 */	"¥¥¥Invalid selector¥¥¥",
/* 114 */	"¥¥¥Invalid selector¥¥¥",
/* 115 */	"¥¥¥Invalid selector¥¥¥",
/* 116 */	"¥¥¥Invalid selector¥¥¥",
/* 117 */	"¥¥¥Invalid selector¥¥¥",
/* 118 */	"¥¥¥Invalid selector¥¥¥",
/* 119 */	"¥¥¥Invalid selector¥¥¥",
/* 120 */	"¥¥¥Invalid selector¥¥¥",
/* 121 */	"¥¥¥Invalid selector¥¥¥",
/* 122 */	"¥¥¥Invalid selector¥¥¥",
/* 123 */	"¥¥¥Invalid selector¥¥¥",
/* 124 */	"¥¥¥Invalid selector¥¥¥",
/* 125 */	"¥¥¥Invalid selector¥¥¥",
/* 126 */	"¥¥¥Invalid selector¥¥¥",
/* 127 */	"cKClientDebugInfo"
};

void PrintAllInfo (void)
{
	OSErr err;
	SKClientDebugInfo* debugInfo;

	err = GetDebugInfo (&debugInfo);
	if (err == noErr) {
		switch (debugInfo -> version) {
			case KClientDebugInfo_Version1:
				PrintSelectorsCalled (debugInfo);
				break;
			
			default:
				printf ("Unknown DebugInfo version returned from KClient.%n");
		}
	} else {
		printf ("Got error %d from KClient when trying to get debug info.", err);
	}
}

OSErr GetDebugInfo (SKClientDebugInfo** outDebugInfoPtr)
{
	ParamBlockRec pbRec;
	short refNum;
	OSErr err;
	
	refNum = IsDriverOpen(KCLIENTDRIVER);
	if (refNum==0)
		return dInstErr;

	pbRec.cntrlParam.ioCompletion = nil;
	pbRec.cntrlParam.ioVRefNum = 0;
	pbRec.cntrlParam.ioCRefNum = refNum;
	pbRec.cntrlParam.csCode = cKClientDebugInfo;
		
	(void) PBControlSync( &pbRec );
	err = pbRec.cntrlParam.ioResult;
	if (err == noErr)
		*outDebugInfoPtr = *(SKClientDebugInfo**)(pbRec.cntrlParam.csParam);
		
	return err;
}

void ClearSelectorsCalled (SKClientDebugInfo* inDebugInfo)
{
	ClearAllSelectorBits_ (inDebugInfo -> selectorsCalled);
}

void PrintSelectorsCalled (SKClientDebugInfo* inDebugInfo)
{
	int i;
	
	printf ("Selectors called:\n");
	for (i = 0; i < 128; i++) {
		if (TestSelectorBit_ (inDebugInfo -> selectorsCalled, i)) {
			printf ("[%3d] %s\n", i, selectorNames [i]);
		}
	}
}

void PrintSelectorsDebugged (SKClientDebugInfo* inDebugInfo)
{
	int i;
	
	printf ("Selectors tripping MacsBug:\n");
	for (i = 0; i < 128; i++) {
		if (TestSelectorBit_ (inDebugInfo -> selectorsDebugged, i)) {
			printf ("[%3d] %s\n", i, selectorNames [i]);
		}
	}
}

void SetAllSelectorsDebugged (SKClientDebugInfo* inDebugInfo)
{
	UInt32 selectors [4];
	SetAllSelectorBits_ (selectors);
	SetSelectorsDebugged (inDebugInfo, selectors, true);
}

void ClearAllSelectorsDebugged (SKClientDebugInfo* inDebugInfo)
{
	UInt32 selectors [4];
	SetAllSelectorBits_ (selectors);
	SetSelectorsDebugged (inDebugInfo, selectors, false);
}

void SetSelectorDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selector)
{
	UInt32 selectors [4];
	ClearAllSelectorBits_ (selectors);
	SetSelectorBit_ (selectors, selector);
	SetSelectorsDebugged (inDebugInfo, selectors, true);
}

void ClearSelectorDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selector)
{
	UInt32 selectors [4];
	ClearAllSelectorBits_ (selectors);
	SetSelectorBit_ (selectors, selector);
	SetSelectorsDebugged (inDebugInfo, selectors, false);
}

void SetSelectorsDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selectors [4], Boolean state)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (state) {
			inDebugInfo -> selectorsDebugged [i] |= selectors [i];
		} else {
			inDebugInfo -> selectorsDebugged [i] &= ~(selectors [i]);
		}
	}
}

/*---------------------------------------------------------------------------------------------------*/
// Stolen from driverInstall.c by jonh 5/21/96. Lots of commented-out debug stuff
// cropped for brevity.

// Re-stolen from IsDriverOpen.c, from developer TC May 1998 CD by meeroh 5/27/1998

#include <Memory.h>
#include <Devices.h>
#include <LowMem.h>

short IsDriverOpen(StringPtr driverName);
DCtlHandle FindTheDriver(StringPtr driverName);

// Return driver ref if driver is open

short IsDriverOpen(StringPtr driverName)
{
	short		dref;
	DCtlHandle	dceHndl;
	
	dref = 0;
	
	dceHndl = FindTheDriver(driverName);
	if (dceHndl != NULL) {
		
		if ((*dceHndl)->dCtlFlags & dOpenedMask)		//	if open (bit 5)
			dref = (*dceHndl)->dCtlRefNum;
	}
	
	return dref;
}


//	FindTheDriver
//	Return driver dctlhandle if it exists.

DCtlHandle FindTheDriver(StringPtr driverName)
{
	DCtlHandle	EntryHand;
	short		count;
	DCtlHandle	*utable;
	
	EntryHand = NULL;

//	number of entries in unit table.  LMGetUnitTableEntryCount isn't defined
//  for PowerPC, but this does the equivalent.	
	count = GetPtrSize(LMGetUTableBase()) / sizeof(DCtlHandle);	
	utable = (DCtlEntry ***) LMGetUTableBase();
	
	while (--count >= 0) {
		DCtlHandle	entry;
		
		entry = *utable++;
		if (entry != NULL) {
			StringPtr	namePtr;
			
		//	see if ram based (test bit 6)
			
			if ((*entry)->dCtlFlags & dRAMBasedMask) {
				
			//	in ram, so we have a handle
				namePtr = (StringPtr) (*(Handle)((*entry)->dCtlDriver)) + 18;
				
			} else {
				
			//	in rom, so we have a pointer
				namePtr = (StringPtr) ((*entry)->dCtlDriver) + 18;
			}
			
			if (RelString(driverName, namePtr, false, true) == 0) {
				
				EntryHand = entry;
				break;
			}
		}
	}
	
	return (EntryHand);
}

