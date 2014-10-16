/*
 * clutils.c - common CL app-level routines, X version
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssm.h>
#include "clutils.h"
#include <Security/cssmapple.h>		/* apple, not intel */
#include <utilLib/common.h>

static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };

static CSSM_VERSION vers = {2, 0};

/*
 * Init CSSM and establish a session with the Apple CL.
 */
CSSM_CL_HANDLE clStartup()
{
	CSSM_CL_HANDLE clHand;
	CSSM_RETURN crtn;
	
	if(cssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509CL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		printError("CSSM_ModuleLoad(AppleCL)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleX509CL,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_CL,		// SubserviceFlags - Where is this used?
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&clHand);
	if(crtn) {
		printError("CSSM_ModuleAttach(AppleCL)", crtn);
		return 0;
	}
	else {
		return clHand;
	}
}

void clShutdown(
	CSSM_CL_HANDLE clHand)
{
	CSSM_RETURN crtn;
	
	crtn = CSSM_ModuleDetach(clHand);
	if(crtn) {
		printf("Error detaching from AppleCL\n");
		printError("CSSM_ModuleDetach", crtn);
		return;
	}
	crtn = CSSM_ModuleUnload(&gGuidAppleX509CL, NULL, NULL);
	if(crtn) {
		printf("Error unloading AppleCL\n");
		printError("CSSM_ModuleUnload", crtn);
	}
}

/*
 * Init CSSM and establish a session with the Apple TP.
 */
CSSM_TP_HANDLE tpStartup()
{
	CSSM_TP_HANDLE tpHand;
	CSSM_RETURN crtn;
	
	if(cssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509TP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		printError("CSSM_ModuleLoad(AppleTP)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleX509TP,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_TP,		// SubserviceFlags
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&tpHand);
	if(crtn) {
		printError("CSSM_ModuleAttach(AppleTP)", crtn);
		return 0;
	}
	else {
		return tpHand;
	}
}

void tpShutdown(
	CSSM_TP_HANDLE tpHand)
{
	CSSM_RETURN crtn;
	
	crtn = CSSM_ModuleDetach(tpHand);
	if(crtn) {
		printf("Error detaching from AppleTP\n");
		printError("CSSM_ModuleDetach", crtn);
		return;
	}
	crtn = CSSM_ModuleUnload(&gGuidAppleX509TP, NULL, NULL);
	if(crtn) {
		printf("Error unloading AppleTP\n");
		printError("CSSM_ModuleUnload", crtn);
	}
}


/*
 * Cook up a CSSM_DATA with specified integer, DER style (minimum number of
 * bytes, big-endian).
 */
CSSM_DATA_PTR intToDER(unsigned theInt)
{
	CSSM_DATA_PTR DER_Data = (CSSM_DATA_PTR)CSSM_MALLOC(sizeof(CSSM_DATA));

	if(theInt < 0x100) {
		DER_Data->Length = 1;
		DER_Data->Data = (uint8 *)CSSM_MALLOC(1);
		DER_Data->Data[0] = (unsigned char)(theInt);
	}
	else if(theInt < 0x10000) {
		DER_Data->Length = 2;
		DER_Data->Data = (uint8 *)CSSM_MALLOC(2);
		DER_Data->Data[0] = (unsigned char)(theInt >> 8);
		DER_Data->Data[1] = (unsigned char)(theInt);
	}
	else if(theInt < 0x1000000) {
		DER_Data->Length = 3;
		DER_Data->Data = (uint8 *)CSSM_MALLOC(3);
		DER_Data->Data[0] = (unsigned char)(theInt >> 16);
		DER_Data->Data[1] = (unsigned char)(theInt >> 8);
		DER_Data->Data[2] = (unsigned char)(theInt);
	}
	else  {
		DER_Data->Length = 4;
		DER_Data->Data = (uint8 *)CSSM_MALLOC(4);
		DER_Data->Data[0] = (unsigned char)(theInt >> 24);
		DER_Data->Data[1] = (unsigned char)(theInt >> 16);
		DER_Data->Data[2] = (unsigned char)(theInt >> 8);
		DER_Data->Data[3] = (unsigned char)(theInt);
	}
	return DER_Data;
}

/*
 * Convert a CSSM_DATA_PTR, referring to a DER-encoded int, to a
 * uint32.
 */
uint32 DER_ToInt(const CSSM_DATA *DER_Data)
{
	uint32		rtn = 0;
	unsigned	i = 0;

	while(i < DER_Data->Length) {
		rtn |= DER_Data->Data[i];
		if(++i == DER_Data->Length) {
			break;
		}
		rtn <<= 8;
	}
	return rtn;
}

