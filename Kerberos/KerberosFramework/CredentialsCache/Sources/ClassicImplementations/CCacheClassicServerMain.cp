#include <kvm.h>
#include <fcntl.h>
#include <sys/sysctl.h>
#include <mach/mach_traps.h>
#include <mach/mach_error.h>
#include <unistd.h>
#include "AEClassicWorkaround.h"

#include "ClassicProtocol.h"
#include "HandleBuffer.h"
#include "ContextDataClassicIntf.h"
#include "CCacheDataClassicIntf.h"
#include "CredsDataClassicIntf.h"
#include "CredentialsCacheInternal.h"

OSErr InstallAppleEventHandlers (void);
pascal OSErr DoOpenApp (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference);
pascal OSErr
DoOpenDoc (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference);
pascal OSErr
DoPrintDoc (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference);
pascal OSErr
DoQuitApp (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference);
pascal OSErr
DoCCacheMessage (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference);

/***
 * 
 * Constants
 * 
 ***/

const UInt32	kAdditionalStackSize		= 24*1024;

/* Sleep time passed to WaitNextEvent. */
const SInt32	kEventLoopTime				= 1;

struct SGlobals {
	Boolean			allDone;	/* true while we are still processing events */
	cc_context_t	context;
};

SGlobals gGlobals;

int main (void)
{
	OSErr				err = noErr;
	EventRecord			theEvent;
	
	/*
	 * Initialize globals
	 */
	gGlobals.allDone = false;

	/*
	 * Install required AppleEvent handlers
	 */
	if (err == noErr) {
		err = InstallAppleEventHandlers ();
	}
	
	while (!gGlobals.allDone) {
		WaitNextEvent (everyEvent, &theEvent, (UInt32) kEventLoopTime, NULL);
		if (theEvent.what == kHighLevelEvent) {
			AEProcessAppleEvent (&theEvent);
		}
	}

	return 0;
}

OSErr InstallAppleEventHandlers (void)
{
	OSErr	err = noErr;
	AEEventHandlerUPP	eventHandlerUPP;

	/*	get the UPPs and install the handlers */
	/*	required events */
	AEEventHandlerUPP	workaroundUPP = NewAEEventHandlerUPP (ClassicReplyWorkaround);
	eventHandlerUPP = NewAEEventHandlerUPP (DoOpenApp);	
	err = AEInstallEventHandler (kCoreEventClass, kAEOpenApplication, eventHandlerUPP, 0, false);
	if (err != noErr)
		return err;
	eventHandlerUPP = NewAEEventHandlerUPP (DoOpenDoc);
	err = AEInstallEventHandler (kCoreEventClass, kAEOpenDocuments, eventHandlerUPP, 0, false);
	if (err != noErr)
		return err;
	eventHandlerUPP = NewAEEventHandlerUPP (DoPrintDoc);
	err = AEInstallEventHandler (kCoreEventClass, kAEPrintDocuments, eventHandlerUPP, 0, false);
	if (err != noErr)
		return err;
	eventHandlerUPP = NewAEEventHandlerUPP (DoQuitApp);
	err = AEInstallEventHandler (kCoreEventClass, kAEQuitApplication, eventHandlerUPP, 0, false);
	if (err != noErr)
		return err;

	/* ccache event */
	eventHandlerUPP = NewAEEventHandlerUPP (DoCCacheMessage);
	err = AEInstallEventHandler (ccClassic_EventClass, ccClassic_EventID, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;

	return err;
}

pascal OSErr
DoOpenApp (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference)
{
#pragma unused(inRequest)
#pragma unused(outReply)
#pragma unused(inReference)

	return noErr;
}

pascal OSErr
DoOpenDoc (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference)
{
#pragma unused (inRequest)
#pragma unused (outReply)
#pragma unused (inReference)

	return noErr;
}

pascal OSErr
DoPrintDoc (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference)
{
#pragma unused(inRequest)
#pragma unused(outReply)
#pragma unused(inReference)

	return noErr;
}

pascal OSErr
DoQuitApp (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference)
{
#pragma unused(inRequest)
#pragma unused(outReply)
#pragma unused(inReference)

	gGlobals.allDone = true;
	return noErr;
}

pascal OSErr
DoCCacheMessage (
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	SInt32 inReference)
{
	OSErr	err = noErr;
	
	UInt32	requestID;
	
	if (err == noErr) {
		Size		size;
		OSType		type;
		err = AEGetKeyPtr (inRequest, ccClassic_Key_MessageID, typeMagnitude, &type, &requestID, sizeof (requestID), &size);
	}
	
	if (err == noErr) {
        
		
		cc_int32 ccErr = ccNoError;
		if (gGlobals.context == NULL) {
			ccErr = cc_initialize (&gGlobals.context, ccapi_version_4, NULL, NULL);
		}
		
		if (ccErr != ccNoError) {
			err = paramErr;
		} else {
			if ((ccClassic_Context_FirstMessage < requestID) && (requestID < ccClassic_Context_LastMessage)) {
				CCIContextDataClassicInterface	context (requestID, inRequest, outReply);
				context.HandleEvent ();
			} else if ((ccClassic_CCache_FirstMessage < requestID) && (requestID < ccClassic_CCache_LastMessage)) {
				CCICCacheDataClassicInterface	ccache (requestID, inRequest, outReply);
				ccache.HandleEvent ();
			} else if ((ccClassic_Credentials_FirstMessage < requestID) && (requestID < ccClassic_Credentials_LastMessage)) {
				CCICredentialsDataClassicInterface	creds (requestID, inRequest, outReply);
				creds.HandleEvent ();
			}
		}

		if (gGlobals.context != NULL) {
			cc_context_release (gGlobals.context);
                        gGlobals.context = NULL;
		}
	}
	
	return err;
}

