/* $Copyright:
 *
 * Copyright 1998-2000 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require a
 * specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and distribute
 * this software and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of M.I.T. not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Furthermore if you
 * modify this software you must label your software as modified software
 * and not distribute it in such a fashion that it might be confused with
 * the original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */

/* $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginHelper/KLHServer.cp,v 1.23 2003/03/17 20:51:39 lxs Exp $ */

/*
 * LoginHelper.c
 *
 * The Login Helper application calls the login library to present the login dialogs for the
 * benefit of faceless background applications. It's driven by AppleEvents.
 */

#include <Kerberos/KerberosLogin.h>
#include <Kerberos/KerberosLoginPrivate.h>
#include <Kerberos/KerberosDebug.h>
#include <Kerberos/AEClassicWorkaround.h>

#include "KerberosLoginHelper.h"


/* Sleep time passed to WaitNextEvent.  */
const SInt32	kEventLoopTime				= 10;

/*
 * Structure used to pass in a principal in an AE
 */

struct SPrincipal {
	KLKerberosVersion	version;
	char 				principal [1];
};

typedef struct SPrincipal SPrincipal;

/*
 * A function to install required AppleEvent handlers
 */

OSErr InstallAppleEventHandlers (void);

/*
 * AppleEvent handlers
 */

/* Handlers for required AppleEvents */
 
pascal OSErr DoOpenApp					(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoOpenDoc					(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoPrintDoc					(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoQuitApp					(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoAcquireNewInitialTickets	(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoChangePassword 			(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoHandleError 				(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoCancelAllDialogs			(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);
pascal OSErr DoPrompter 				(const AppleEvent *inRequest, AppleEvent *outReply, long inReference);


/* AE utility functions */
pascal Boolean HLEComparator (EventRef inEvent, void *inCompareData);
KLStatus	GotRequiredParams (const AppleEvent* theAppleEvent);
KLBoolean	gAllDone;			/* false while we are still processing events */

#pragma mark -
/*
 * Wait for Open Application event and then do what is needed
 */

int main (void) 
{
	KLStatus			err = noErr;
	EventRecord			theEvent;
	
	gAllDone = false;
	
	/*
	 * Install required AppleEvent handlers
	 */
	if (err == noErr) {
		err = InstallAppleEventHandlers ();
        dprintf ("InstallAppleEventHandlers returned (err = %ld)\n", err);
	}
	
    EventComparatorUPP		hleComparatorUPP = NULL;
	if (err == noErr) {
        hleComparatorUPP = NewEventComparatorUPP (HLEComparator);
        if (hleComparatorUPP == nil) {
            err = memFullErr;
        }
	}
 	static	UInt32		lastAETime = TickCount ();
	bool	deferQuit = false;			// This is true when we get a quit event but we have another
 										// AE pending that we need to service before quitting
	while (!gAllDone || deferQuit) {
 		/*
		 * Under certain circumstances, interaction between the Notification Manager
		 * and the Apple Event Manager can cause Quit events to be deferred -- but we
		 * really do want to quit in those cases, after handling login events. Therefore,
		 * 1 second after we are done handling the login events, we quit no matter what
		 */
		WaitNextEvent (everyEvent, &theEvent, (UInt32) kEventLoopTime, nil);
		if (theEvent.what == kHighLevelEvent) {
            dprintf ("Kerberos Login Helper processing AppleEvent.\n");
			AEProcessAppleEvent (&theEvent);
            dprintf ("Kerberos Login Helper processed AppleEvent.\n");
			lastAETime = TickCount ();
		}
		if (lastAETime + 3600 /* 1 minute */ < TickCount ()) {
			gAllDone = true;
		}
        deferQuit = (FindSpecificEventInQueue (GetMainEventQueue (), hleComparatorUPP, NULL) != NULL);
	}
    dprintf ("KerberosLoginHelper exiting with error code %ld\n", err);
    return err;
}

OSErr
InstallAppleEventHandlers (void)
{
	OSErr	err = noErr;
	AEEventHandlerUPP	eventHandlerUPP;

	/* Get the UPPs and install the handlers */

	/* Required events */
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
		
	/* Login Helper events */
	AEEventHandlerUPP workaroundUPP = NewAEEventHandlerUPP (ClassicReplyWorkaround);	
	eventHandlerUPP = NewAEEventHandlerUPP (DoAcquireNewInitialTickets);	
	err = AEInstallEventHandler (kKLHEventClass, kAEAcquireNewInitialTickets, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;
	
	eventHandlerUPP = NewAEEventHandlerUPP (DoChangePassword);
	err = AEInstallEventHandler (kKLHEventClass, kAEChangePassword, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;

	eventHandlerUPP = NewAEEventHandlerUPP (DoHandleError);
	err = AEInstallEventHandler (kKLHEventClass, kAEHandleError, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;

	eventHandlerUPP = NewAEEventHandlerUPP (DoCancelAllDialogs);
	err = AEInstallEventHandler (kKLHEventClass, kAECancelAllDialogs, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;

	eventHandlerUPP = NewAEEventHandlerUPP (DoPrompter);
	err = AEInstallEventHandler (kKLHEventClass, kAEPrompter, workaroundUPP, (SInt32) eventHandlerUPP, false);
	if (err != noErr)
		return err;

	return err;
}

#pragma mark -

pascal OSErr
DoOpenApp (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
#pragma unused(outReply)
#pragma unused(inReference)

	return GotRequiredParams (inRequest);
}

pascal OSErr
DoOpenDoc (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
#pragma unused (outReply)
#pragma unused (inReference)

	return GotRequiredParams (inRequest);
}

pascal OSErr
DoPrintDoc (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
#pragma unused(outReply)
#pragma unused(inReference)

	return GotRequiredParams (inRequest);
}

pascal OSErr
DoQuitApp (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
#pragma unused(outReply)
#pragma unused(inReference)

	gAllDone = true;
	return GotRequiredParams (inRequest);
}


pascal OSErr
DoAcquireNewInitialTickets (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
	KLPrincipal		principal = nil;
	KLPrincipal		outPrincipal = nil;
	char*			outCacheName = nil;
	char*			outPrincipalString = nil;
	
	OSErr			handlerErr = noErr;
	KLStatus		replyErr = noErr;
	
	Size 			principalSize = 0;
	DescType		principalType;

	DescType		actualType;
	Size			actualSize;
	
    dprintf ("KerberosLoginHelper got KLAcquireNewInitialTickets event\n");
    
	/* extract the principal */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrincipal, &principalType, &principalSize);
	}

	if ((handlerErr == noErr) && (replyErr == noErr)) {
        Handle			principalHandle = nil;
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            principalHandle = NewHandle (principalSize);
            if (principalHandle == nil ) {
                handlerErr = memFullErr;
            }
        }
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            HLock (principalHandle);
            handlerErr = AEGetParamPtr (inRequest, keyKLPrincipal, principalType, &actualType, 
                                        *principalHandle, principalSize, &actualSize);
        }
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            char *principalString = *(char **) principalHandle;
            dprintf ("Kerberos Login Helper got a principal: '%s'\n", principalString);
            handlerErr = KLCreatePrincipalFromString (principalString, kerberosVersion_V5, &principal);
        }
        
        if (principalHandle != nil) {
            DisposeHandle (principalHandle);
        }
	} else {
        principal = nil;
        handlerErr = noErr;
    }
    
	/* Make sure we didn't miss any parameters */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = GotRequiredParams (inRequest);
        dprintf ("Kerberos Login Helper checking for required parameters (err = %ld)\n", handlerErr);
        if (handlerErr == errAEDescNotFound) {
            /* No missed arguments */
            handlerErr = noErr;
        }
    }
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		replyErr = KLAcquireNewInitialTickets (principal, NULL, &outPrincipal, &outCacheName);
		dprintf ("Kerberos Login Helper called KLAcquireNewInitialTickets (err = %ld).\n", replyErr);
	}
    
    if ((handlerErr == noErr) && (replyErr == noErr)) {
        replyErr = KLGetStringFromPrincipal (outPrincipal, kerberosVersion_V5, &outPrincipalString);
        if (replyErr != noErr) {
            dprintf ("Kerberos Login Helper Application got tickets for principal '%s'\n", outPrincipalString);
        }
	}
    
	if ((handlerErr == noErr) && (replyErr != noErr)) {
		/* KLL returned an error, place that in the Apple Event reply */
		handlerErr = AEPutParamPtr (outReply, keyKLError, typeLongInteger, &replyErr, sizeof (replyErr));
	} else {
        /* KLL returned successfully, place the principal and cache name in the reply */
        if (handlerErr == noErr) {
            dprintf ("Kerberos Login Helper returning principal '%s'\n", outPrincipalString);
            handlerErr = AEPutParamPtr (outReply, keyKLPrincipal, typeKLPrincipalString, 
                                        outPrincipalString, (Size)(strlen (outPrincipalString) + 1));
        }

        if (handlerErr == noErr) {
            dprintf ("Kerberos Login Helper returning cache name '%s'\n", outCacheName);
            handlerErr = AEPutParamPtr (outReply, keyKLCacheName, typeKLCacheName, 
                                        outCacheName, (Size)(strlen (outCacheName) + 1));
        }
	}

	if (principal != nil) {
		KLDisposePrincipal (principal);
	}
	
	if (outPrincipal != nil) {
		KLDisposePrincipal (outPrincipal);
	}
	
	if (outCacheName != nil) {
		KLDisposeString (outCacheName);
	}
	
	if (outPrincipalString != nil) {
		KLDisposeString (outPrincipalString);
	}
	
	if (handlerErr != noErr) {
		dprintf ("Handler error is %d\n", handlerErr);
	}

	if (replyErr != noErr) {
		dprintf ("Reply error is %d\n", replyErr);
	}

	#if DCON	
	dprintae (outReply);
	#endif

	return handlerErr;
}


pascal OSErr
DoChangePassword (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
	KLPrincipal		principal = nil;
	
	OSErr			handlerErr = noErr;
	KLStatus		replyErr = noErr;
	
	Size 			principalSize = 0;
	DescType		principalType;

	DescType		actualType;
	Size			actualSize;
	
	/* extract the principal */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrincipal, &principalType, &principalSize);
        dprintf ("AESizeOfParam returned (err = %d)\n", handlerErr);
    }
    
    if ((handlerErr == noErr) && (replyErr == noErr)) {
        Handle			principalHandle = nil;
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            principalHandle = NewHandle (principalSize);
            dprintf ("Allocating a handle of size %d (got %x)\n", principalSize, principalHandle);
            if (principalHandle == nil ) {
                handlerErr = memFullErr;
            }
        }
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            HLock (principalHandle);
            handlerErr = AEGetParamPtr (inRequest, keyKLPrincipal, principalType, &actualType, 
                                        *principalHandle, principalSize, &actualSize);
            dprintf ("AEGetParamPtr returned %d\n", handlerErr);
        }
        
        if ((handlerErr == noErr) && (replyErr == noErr)) {
            char *principalString = *(char **) principalHandle;
            dprintf ("Kerberos Login Helper got a principal: '%s'\n", principalString);
            handlerErr = KLCreatePrincipalFromString (principalString, kerberosVersion_V5, &principal);
        }
        
        if (principalHandle != nil) {
            DisposeHandle (principalHandle);
        }
	} else {
        principal = nil;
        handlerErr = noErr;
    }
    
	/* Make sure we didn't miss any parameters */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = GotRequiredParams (inRequest);
        dprintf ("Kerberos Login Helper checking for required parameters (err = %ld)\n", handlerErr);
        if (handlerErr == errAEDescNotFound) {
            /* No missed arguments */
            handlerErr = noErr;
        }
    }
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		replyErr = KLChangePassword (principal);
	}
	
	if ((handlerErr == noErr) && (replyErr != noErr)) {
		/* KLL returned an error, place that in the Apple Event reply */
		handlerErr = AEPutParamPtr (outReply, keyKLError, typeLongInteger, &replyErr, sizeof (replyErr));
	}
	
	if (principal != nil) {
		KLDisposePrincipal (principal);
	}
	
	if (handlerErr != noErr) {
		dprintf ("Handler error is %d\n", handlerErr);
	}

	if (replyErr != noErr) {
		dprintf ("Reply error is %d\n", replyErr);
	}

	#if DCON	
	dprintae (outReply);
	#endif

	return handlerErr;
}

pascal OSErr
DoHandleError (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
	KLStatus			error;
	KLDialogIdentifier	dialogIdentifier;
	KLBoolean			showAlert;
		
	OSErr				handlerErr = noErr;
	KLStatus			replyErr = noErr;
	
	DescType			actualType;
	Size				actualSize;
	
	/* extract the arguments */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AEGetParamPtr (inRequest, keyKLError, typeSInt32, &actualType, 
								&error, sizeof (error), &actualSize);
    }
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLDialogIdentifier, typeUInt32, &actualType, 
									&dialogIdentifier, sizeof (dialogIdentifier), &actualSize);
	}

	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLShowAlert, typeBoolean, &actualType, 
									&showAlert, sizeof (showAlert), &actualSize);
	}
	
	/* Make sure we didn't miss any parameters */
    if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = GotRequiredParams (inRequest);
        dprintf ("Kerberos Login Helper checking for required parameters (err = %ld)\n", handlerErr);
        if (handlerErr == errAEDescNotFound) {
            handlerErr = noErr;
        }
    }

	if ((handlerErr == noErr) && (replyErr == noErr)) {
		replyErr = KLHandleError (error, dialogIdentifier, showAlert);
	}
	
	if ((handlerErr == noErr) && (replyErr != noErr)) {
		/* KLL returned an error, place that in the Apple Event reply */
		handlerErr = AEPutParamPtr (outReply, keyKLError, typeLongInteger, &replyErr, sizeof (replyErr));
	}
	
	if (handlerErr != noErr) {
		dprintf ("Handler error is %d\n", handlerErr);
	}

	if (replyErr != noErr) {
		dprintf ("Reply error is %d\n", replyErr);
	}

	#if DCON	
	dprintae (outReply);
	#endif

	return handlerErr;
}


pascal OSErr
DoCancelAllDialogs (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
	OSErr				handlerErr = noErr;
	KLStatus			replyErr = noErr;
		
	/* Make sure we didn't miss any parameters */
    if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = GotRequiredParams (inRequest);
        dprintf ("Kerberos Login Helper checking for required parameters (err = %ld)\n", handlerErr);
        if (handlerErr == errAEDescNotFound) {
            handlerErr = noErr;
        }
    }
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		replyErr = KLCancelAllDialogs ();
	}
	
	if ((handlerErr == noErr) && (replyErr != noErr)) {
		/* KLL returned an error, place that in the Apple Event reply */
		handlerErr = AEPutParamPtr (outReply, keyKLError, typeLongInteger, &replyErr, sizeof (replyErr));
	}
	
	if (handlerErr != noErr) {
		dprintf ("Handler error is %d\n", handlerErr);
	}

	if (replyErr != noErr) {
		dprintf ("Reply error is %d\n", replyErr);
	}

#if DCON	
	dprintae (outReply);
#endif

	return handlerErr;
}


pascal OSErr
DoPrompter (const AppleEvent *inRequest, AppleEvent *outReply, long inReference)
{
	char				*name = NULL;
	char				*banner = NULL;
	int					 num_prompts;
	krb5_prompt			*prompts = NULL;
	
	char				*promptStrings = NULL;
	char				*promptHiddenFlags = NULL;
	krb5_data			*promptReplies = NULL;
	int					*promptReplyMaxSizes = NULL;
	char				*promptReplyData = NULL;
    int					 promptReplyDataSize = 0;

	int					 i;
	
	OSErr			 	 handlerErr = noErr;
	KLStatus			 replyErr = noErr;
	
	DescType			 argumentType;
	DescType			 actualType;
	Size				 argumentSize;
	Size				 actualSize;
	
	/* Extract the name argument */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrompterName, &argumentType, &argumentSize);
	}
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		name = (char *) malloc (argumentSize);
		if (name == NULL) {
			handlerErr = memFullErr;
		}
		
		if (handlerErr == noErr) {
			handlerErr = AEGetParamPtr (inRequest, keyKLPrompterName, argumentType, &actualType, 
										name, argumentSize, &actualSize);
		}
	} else {
		/* The name is NULL */
		handlerErr = noErr;
	}
	

	/* Extract the banner argument */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrompterBanner, &argumentType, &argumentSize);
    }	
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		banner = (char *) malloc (argumentSize);
		if (banner == NULL) {
			handlerErr = memFullErr;
		}
		
		if ((handlerErr == noErr) && (replyErr == noErr)) {
			handlerErr = AEGetParamPtr (inRequest, keyKLPrompterBanner, argumentType, &actualType, 
										banner, argumentSize, &actualSize);
		}
	} else {
		/* The banner is NULL */
		handlerErr = noErr;
	}
	
	/* Extract the number of prompts */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLPrompterNumPrompts, typeKLPrompterNumPrompts, &actualType, 
									&num_prompts, sizeof (num_prompts), &actualSize);
	}
		
	/* Extract the prompt strings */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrompterPromptStrings, &argumentType, &argumentSize);
	}
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		promptStrings = (char *) malloc (argumentSize);
		if (promptStrings == NULL) {
			handlerErr = memFullErr;
		}
	}
		
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLPrompterPromptStrings, argumentType, &actualType, 
										promptStrings, argumentSize, &actualSize);
	}
	
	/* Extract the hidden strings */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrompterPromptHidden, &argumentType, &argumentSize);
	}
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		promptHiddenFlags = (char *) malloc (argumentSize);
		if (promptHiddenFlags == NULL) {
			handlerErr = memFullErr;
		}
	}
		
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLPrompterPromptHidden, argumentType, &actualType, 
										promptHiddenFlags, argumentSize, &actualSize);
	}

	/* Extract the max sizes of the replies */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = AESizeOfParam (inRequest, keyKLPrompterReplyMaxSizes, &argumentType, &argumentSize);
	}
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		promptReplyMaxSizes = (int *) malloc (argumentSize);
		if (promptReplyMaxSizes == NULL) {
			handlerErr = memFullErr;
		}
	}
		
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		handlerErr = AEGetParamPtr (inRequest, keyKLPrompterReplyMaxSizes, argumentType, &actualType, 
										promptReplyMaxSizes, argumentSize, &actualSize);
	}

	/* Create the array of prompts */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		prompts = (krb5_prompt *) malloc (num_prompts * sizeof (krb5_prompt));
		if (prompts == NULL) 
			handlerErr = memFullErr;
	}
	
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		promptReplies = (krb5_data *) malloc (num_prompts * sizeof (krb5_data));
		if (promptReplies == NULL) 
			handlerErr = memFullErr;
	}

	if ((handlerErr == noErr) && (replyErr == noErr)) {
        for (i = 0; i < num_prompts; i++)
            promptReplyDataSize += promptReplyMaxSizes[i];
        
		promptReplyData = (char *) malloc (promptReplyDataSize);
		if (promptReplyData == NULL) 
			handlerErr = memFullErr;
	}
	
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		char	*currentPromptString = promptStrings;
		char	*currentPromptReplyData = promptReplyData;
		
		for (i = 0; i < num_prompts; i++) {
			prompts[i].prompt = currentPromptString;
			prompts[i].hidden = promptHiddenFlags[i] == '1' ? true : false;
			
			prompts[i].reply = &promptReplies[i];
			prompts[i].reply->length = promptReplyMaxSizes[i];
			prompts[i].reply->data = currentPromptReplyData;
			
			currentPromptString += strlen (currentPromptString) + 1;
			currentPromptReplyData += promptReplyMaxSizes[i];
		}
	}
	
	/* Make sure we didn't miss any parameters */
	if ((handlerErr == noErr) && (replyErr == noErr)) {
        handlerErr = GotRequiredParams (inRequest);
        dprintf ("Kerberos Login Helper checking for required parameters (err = %ld)\n", handlerErr);
        if (handlerErr == errAEDescNotFound) {
            handlerErr = noErr;
        }
    }
    
	if ((handlerErr == noErr) && (replyErr == noErr)) {
		replyErr = __KLPrompter (NULL /* context */, NULL /* data */, name, banner, num_prompts, prompts);
	}
	
	if ((handlerErr == noErr) && (replyErr != noErr)) {
		/* KLL returned an error, place that in the Apple Event reply */
		handlerErr = AEPutParamPtr (outReply, keyKLError, typeLongInteger, &replyErr, sizeof (replyErr));
	}
	
	if (handlerErr != noErr) {
		dprintf ("Handler error is %d\n", handlerErr);
	}

	if (replyErr != noErr) {
		dprintf ("Reply error is %d\n", replyErr);
	}

#if DCON	
	dprintae (outReply);
#endif

	/* Cleanup */
	if (name != NULL)
		free (name);

	if (banner != NULL)
		free (banner);

	if (prompts != NULL)
		free (prompts);

	if (promptStrings != NULL)
		free (promptStrings);

	if (promptHiddenFlags != NULL)
		free (promptHiddenFlags);

	if (promptReplies != NULL)
		free (promptReplies);

	if (promptReplyMaxSizes != NULL)
		free (promptReplyMaxSizes);

	if (promptReplyData != NULL)
		free (promptReplyData);

	return handlerErr;
}


#pragma mark -

pascal Boolean HLEComparator (EventRef inEvent, void *inCompareData)
{
    // true if we have an apple event
    return (GetEventClass (inEvent) == kEventClassAppleEvent);
}

// ----------------------------------------------------------------------
//	Name:		GotRequiredParams
//	Function:	Checks all parameters defined as 'required' have been read
// ----------------------------------------------------------------------
							
KLStatus
GotRequiredParams(const AppleEvent *theAppleEvent)
{
	DescType	returnedType;
	Size		actualSize;
	OSErr		err;
	AEKeyword	missedParam;
	
		// look for the keyMissedKeywordAttr, just to see if it's there
	
	err = AEGetAttributePtr(theAppleEvent, keyMissedKeywordAttr, typeKeyword,
		&returnedType, &missedParam, sizeof (missedParam), &actualSize);
	
	switch (err)
	{
		case errAEDescNotFound:		// attribute not there means we
			err = noErr;			// got all required parameters.
			break;
			
		case noErr:					// attribute there means missed
			err = errAEParamMissed;	// at least one parameter.
			break;
			
		// default:		pass on unexpected error in looking for the attribute
	}
	
	return(err);
} // GotReqiredParams
