#include <string.h>

#include <Kerberos/KerberosSupport.h>
#include <Kerberos/AEClassicWorkaround.h>
#include <Kerberos/CredentialsCacheInternal.h>

#include <Carbon/Carbon.h>

#include "KerberosLoginHelper.h"
#include "UString.h"
#include "UKLEnvironment.h"
#include "KLAppearanceIdleCursor.h"
#include "KLApplicationOptions.h"
#include <unistd.h>

static KLBoolean KLHIdleHaveEventFilter ();
static pascal KLBoolean KLHIdleCallback (
    EventRecord *theEvent, 
    SInt32 *sleepTime, 
    RgnHandle *mouseRgn);

static KLStatus QuitKerberosLoginHelper (AEDesc *helperDesc);
static KLStatus LaunchKerberosLoginHelper (AEDesc	*helperDesc);
   
static AEIdleUPP 				GetKLHIdleCallbackUPP (void);
static AEIdleUPP 				gKLHIdleCallbackUPP = NULL;
static KLAppearanceIdleCursor 	*gIdleCursor = NULL;


static KLBoolean
KLHIdleHaveEventFilter ()
{
	KLApplicationOptions options;
	
	UKLApplicationOptions::GetApplicationOptions (&options);
	return (options.eventFilter != nil);
}

static pascal Boolean 
KLHIdleCallback (
    EventRecord *theEvent, 
    SInt32 *sleepTime, 
    RgnHandle *mouseRgn)
{
	KLApplicationOptions options;
	
	UKLApplicationOptions::GetApplicationOptions (&options);
	if ((options.eventFilter) == nil && (gIdleCursor != NULL)) {
		gIdleCursor->IdleCursor ();
	}

	IdleHandleEvent (theEvent);
#warning Shouldnt hard code value for event sleep
	*sleepTime = 10;
	*mouseRgn = NULL;
	
	return false;
}

static AEIdleUPP 
GetKLHIdleCallbackUPP (void)
{
	if (gKLHIdleCallbackUPP == NULL) {
		gKLHIdleCallbackUPP = NewAEIdleUPP (KLHIdleCallback);
		Assert_ (gKLHIdleCallbackUPP != NULL);
	}
	return gKLHIdleCallbackUPP;
}

void 
DisposeKLHIdleCallbackUPP (void)
{
	if (gKLHIdleCallbackUPP != NULL)
		DisposeAEIdleUPP (gKLHIdleCallbackUPP);
}

static KLStatus
LaunchKerberosLoginHelper (
	AEDesc	*helperDesc)
{
	KLStatus 			err = noErr;
	LaunchParamBlockRec	launchParams;
    FSSpec				kerberosLoginHelperSpec;
    ProcessInfoRec		pir;
        
    // If we are running under Classic and the KLH in X has a matching version...
    if (::RunningUnderClassic () && (::CheckClassicIntegrationCompatibility () == noErr)) {
        // We need to launch the yellow-side Kerberos Login Helper
		// Before we launch it, we have to find it, and to do that 
		// we have to discover where the system is. We do that by looking
		// at where the Finder is, since there is no other way that I know
		// of to find the X volume from Classic
		
		static bool sHaveYellowVolume = false;
		static SInt16 sYellowVolumeRefNum = 0;
		
		if (!sHaveYellowVolume) {
			FSSpec appSpec;
			pir.processInfoLength = sizeof (pir);
			pir.processAppSpec = &appSpec;
			pir.processName = nil;
			ProcessSerialNumber psn = {0, kNoProcess};
            
			while (GetNextProcess (&psn) == noErr) {
				if (GetProcessInformation (&psn, &pir) == noErr) {
					if ((pir.processSignature == 'MACS') && (pir.processType == 'FNDR')) {
						sHaveYellowVolume = true;
						sYellowVolumeRefNum = appSpec.vRefNum;
						break;
					}
				}
			}
		}
		
		if (!sHaveYellowVolume) {
			return fnfErr;
		}
		
		err = FSMakeFSSpec (sYellowVolumeRefNum, fsRtDirID, "\p:System:Library:Frameworks:Kerberos.framework:Versions:A:Servers:KerberosLoginHelper.app:Contents:MacOS:KerberosLoginHelper", &kerberosLoginHelperSpec);
		if (err != noErr) {
			return err;
		}
        launchParams.launchAppSpec = &kerberosLoginHelperSpec;
    } else {
        // We are booted into Mac OS 9... launch the library
        launchParams.launchAppSpec = &gLibraryFile;
    }
    
	launchParams.launchAppParameters	= nil;	
	launchParams.launchBlockID = extendedBlock;
	launchParams.launchEPBLength = extendedBlockLen;
	launchParams.launchFileFlags = 0;
	launchParams.launchControlFlags = launchNoFileFlags | launchContinue;
	launchParams.launchAppParameters = nil;

	err = LaunchApplication (&launchParams);
	dprintf ("Launching Kerberos Login Helper returned '%ld'\n", err);
		
    // Wait for KLH to exist.  
    pir.processInfoLength = sizeof (pir);
	pir.processName = nil;
	pir.processAppSpec = nil;
    while (GetProcessInformation (&launchParams.launchProcessSN, &pir) != noErr) {
        EventRecord event;
        
        WaitNextEvent (everyEvent, &event, 1, nil);
        IdleHandleEvent (&event);
    }

	if (err == noErr) {
		// On classic we use creator-based address because John Montbriand said so
		if (::RunningUnderClassic () && (::CheckClassicIntegrationCompatibility () == noErr)) {
			OSType		helperSignature = FOUR_CHAR_CODE ('KLHa');
			err = AECreateDesc (typeApplSignature, (Ptr) &helperSignature, 
								sizeof (helperSignature), helperDesc);
		} else {
			err = AECreateDesc (typeProcessSerialNumber, (Ptr) &launchParams.launchProcessSN, 
								sizeof (ProcessSerialNumber), helperDesc);
		}
	}
    
	return err;
}

static KLStatus
QuitKerberosLoginHelper (AEDesc *helperDesc)
{		
	OSStatus err = noErr;
	
	// Quit LoginHelper
	AppleEvent	quitEvent = {typeNull, nil};

	/* Quit the Kerberos Login Helper now that we are done */
	err = AECreateAppleEvent (kCoreEventClass, kAEQuitApplication, helperDesc, 
								kAutoGenerateReturnID, kAnyTransactionID, &quitEvent);
	
	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESend (&quitEvent, nil, kAENoReply, kAENormalPriority, 
					kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf("Sending a quit event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (quitEvent.dataHandle != nil)
		AEDisposeDesc (&quitEvent);

	return err;
}	


KLStatus
KLHAcquireNewInitialTickets (
	KLPrincipal		inPrincipal,
	KLPrincipal*	outPrincipal,
	char**			outCacheName)
{
	AppleEvent 	helperEvent = {typeNull, nil};
	AppleEvent 	replyEvent = {typeNull, nil};
	AEDesc 		replyDesc = {typeNull, nil};
	AEDesc 		helperDesc = {typeNull, nil};
	char*		klPrincipalString = nil;
	
	OSStatus 	err = noErr;
	KLStatus	klErr = klNoErr;

	if (gKerberosLoginDialogExists) {
		// There is already a login dialog on screen somewhere
		return klDialogAlreadyExistsErr;
	}

	/* Launch the Kerberos Login Helper and get a AE descriptor for it */	
	err = LaunchKerberosLoginHelper (&helperDesc);

	if (err == noErr) {
		err = AECreateAppleEvent (kKLHEventClass, kAEAcquireNewInitialTickets, &helperDesc, 
									kAutoGenerateReturnID, kAnyTransactionID, &helperEvent);
	}
	
	if ((err == noErr) && (inPrincipal != nil)) {
		err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, &klPrincipalString);
		
		if (err == noErr) {
			err = AEPutParamPtr (&helperEvent, keyKLPrincipal, typeKLPrincipalString, 
									klPrincipalString, (Size)(strlen (klPrincipalString) + 1));
		}
	}
	
#if DCON
	dprintae (&helperEvent);
#endif

	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESendWorkaround (&helperEvent, &replyEvent, kAEWaitReply | kAECanInteract, kAENormalPriority, 
						kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf ("Sending a kAEAcquireNewInitialTickets event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (err == noErr) {
		DescType	actualType;
		Size		actualSize;
		
		/* Check whether we have an error reply or not */
		err = AEGetParamPtr (&replyEvent, keyKLError, typeLongInteger, &actualType, &klErr, sizeof (klErr), &actualSize);
		if (err != noErr) {
			/* KLAcquireNewInitialTickets returned noErr */
			
			if (outPrincipal != nil) {
				/* extract the principal */
				Size 		principalSize = 0;
				DescType	principalType;
				Handle		principalHandle = nil;
				
				err = AESizeOfParam (&replyEvent, keyKLPrincipal, &principalType, &principalSize);
				
				principalHandle = NewHandle (principalSize);
				if (principalHandle == nil ) {
					err = klMemFullErr;
				}
				
				if (err == noErr) {
					HLock (principalHandle);
					err = AEGetParamPtr (&replyEvent, keyKLPrincipal, principalType, &actualType, *principalHandle, principalSize, &actualSize);
				}
				
				if (err == noErr) {
					char *principal = *(char **) principalHandle;
					dprintf ("Kerberos Login Helper returned principal name: '%s'\n", principal);
					err = KLCreatePrincipalFromString (principal, kerberosVersion_V5, outPrincipal);
				}
				
				if (principalHandle != nil) {
					DisposeHandle (principalHandle);
				}
			}
			
			if (outCacheName != nil) {
				/* extract the cache name */
				Size 		cacheNameSize = 0;
				DescType	cacheNameType;
				Handle		cacheNameHandle = nil;
				
				err = AESizeOfParam (&replyEvent, keyKLCacheName, &cacheNameType, &cacheNameSize);
				
				cacheNameHandle = NewHandle (cacheNameSize);
				if (cacheNameHandle == nil ) {
					err = klMemFullErr;
				}
				
				if (err == noErr) {
					HLock (cacheNameHandle);
					err = AEGetParamPtr (&replyEvent, keyKLCacheName, cacheNameType, &actualType, 
                                        *cacheNameHandle, cacheNameSize, &actualSize);
				}
				
				if (err == noErr) {
					char *cacheName = *(char **) cacheNameHandle;
					*outCacheName = UString::NewCString (cacheName);
					dprintf ("Kerberos Login Helper returned cache name: '%s'\n", cacheName);
				}
				
				if (cacheNameHandle != nil) {
					DisposeHandle (cacheNameHandle);
				}
			}
		}
	}
	
	/* Quit Kerberos Login Helper */
	err = QuitKerberosLoginHelper (&helperDesc);
    
    /* Unlike the other KLH operations, this one can modify the CCache.  
       On Classic, we must update the cache synchronously */
    if (::RunningUnderClassic ()) {
		if (KLHIdleHaveEventFilter () == false) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
        __CredentialsCacheInternalSyncWithYellowCache (GetKLHIdleCallbackUPP ());
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
    }

	/* Cleanup memory */
	if (helperDesc.dataHandle != nil)
		AEDisposeDesc (&helperDesc);

	if (helperEvent.dataHandle != nil)
		AEDisposeDesc (&helperEvent);
		
	if (replyDesc.dataHandle != nil)
		AEDisposeDesc (&replyDesc);

	if (replyEvent.dataHandle != nil)
		AEDisposeDesc (&replyEvent);
			
	if (klPrincipalString != nil)
		KLDisposeString (klPrincipalString);
	
	if (err != noErr)
		return err;
	else
		return klErr;
}



KLStatus
KLHChangePassword (
	KLPrincipal		inPrincipal)
{
	AppleEvent 	helperEvent = {typeNull, nil};
	AppleEvent 	replyEvent = {typeNull, nil};
	AEDesc 		replyDesc = {typeNull, nil};
	AEDesc 		principalDesc = {typeNull, nil};
	AEDesc 		helperDesc = {typeNull, nil};
	char*		klPrincipalString = nil;
	
	OSStatus 	err = noErr;
	KLStatus	klErr = klNoErr;

	if (gKerberosLoginDialogExists) {
		// There is already a login dialog on screen somewhere
		return klDialogAlreadyExistsErr;
	}

	/* Launch the Kerberos Login Helper and get a AE descriptor for it */	
	err = LaunchKerberosLoginHelper (&helperDesc);

	if (err == noErr) {
		err = AECreateAppleEvent (kKLHEventClass, kAEChangePassword, &helperDesc, kAutoGenerateReturnID, kAnyTransactionID, &helperEvent);
	}
	
	if ((err == noErr) && (inPrincipal != nil)) {
		err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, &klPrincipalString);
		
		if (err == noErr) {
			err = AEPutParamPtr (&helperEvent, keyKLPrincipal, typeKLPrincipalString, 
                                    klPrincipalString, (Size)(strlen (klPrincipalString) + 1));
		}
	}

#if DCON
	dprintae (&helperEvent);
#endif

	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESendWorkaround (&helperEvent, &replyEvent, kAEWaitReply | kAECanInteract, kAENormalPriority, 
						kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf("Sending a kAEChangePassword event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (err == noErr) {
		DescType	actualType;
		Size		actualSize;
		
		/* Check whether we have an error reply or not */
		err = AEGetParamPtr (&replyEvent, keyKLError, typeLongInteger, &actualType, 
                            &klErr, sizeof (klErr), &actualSize);
	}
		
	/* Quit Kerberos Login Helper */
	err = QuitKerberosLoginHelper (&helperDesc);

	/* Cleanup memory */
	if (helperDesc.dataHandle != nil)
		AEDisposeDesc (&helperDesc);

	if (helperEvent.dataHandle != nil)
		AEDisposeDesc (&helperEvent);
		
	if (principalDesc.dataHandle != nil)
		AEDisposeDesc (&principalDesc);

	if (replyDesc.dataHandle != nil)
		AEDisposeDesc (&replyDesc);

	if (replyEvent.dataHandle != nil)
		AEDisposeDesc (&replyEvent);
			
	if (klPrincipalString != nil)
		KLDisposeString (klPrincipalString);
		
	if (err != noErr)
		return err;
	else
		return klErr;
}


KLStatus
KLHHandleError (
		KLStatus					inError,
		KLDialogIdentifier			inDialogIdentifier,
		KLBoolean					inShowAlert)
{
	AppleEvent 	helperEvent = {typeNull, nil};
	AppleEvent 	replyEvent = {typeNull, nil};
	AEDesc 		replyDesc = {typeNull, nil};
	AEDesc 		helperDesc = {typeNull, nil};
	
	OSStatus 	err = noErr;
	KLStatus	klErr = klNoErr;

	if (gKerberosLoginDialogExists) {
		// There is already a login dialog on screen somewhere
		return klDialogAlreadyExistsErr;
	}

	/* Launch the Kerberos Login Helper and get a AE descriptor for it */	
	err = LaunchKerberosLoginHelper (&helperDesc);

	if (err == noErr) {
		err = AECreateAppleEvent (kKLHEventClass, kAEHandleError, &helperDesc, 
                                    kAutoGenerateReturnID, kAnyTransactionID, &helperEvent);
	}
	
	/* Add the inError argument */
	if (err == noErr) {
		err = AEPutParamPtr (&helperEvent, keyKLError, typeSInt32, &inError, sizeof(inError));
	}
	
	/* Add the inDialogIdentifer argument */
	if (err == noErr) {
		err = AEPutParamPtr (&helperEvent, keyKLDialogIdentifier, typeUInt32, 
                            &inDialogIdentifier, sizeof(inDialogIdentifier));
	}

	/* Add the inShowAlert argument */
	if (err == noErr) {
		err = AEPutParamPtr (&helperEvent, keyKLShowAlert, typeBoolean, &inShowAlert, sizeof(inShowAlert));
	}
	
#if DCON
	dprintae (&helperEvent);
#endif

	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESendWorkaround (&helperEvent, &replyEvent, kAEWaitReply | kAECanInteract, kAENormalPriority, 
						kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf("Sending a kAEHandleError event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (err == noErr) {
		DescType	actualType;
		Size		actualSize;
		
		/* Check whether we have an error reply or not */
		err = AEGetParamPtr (&replyEvent, keyKLError, typeLongInteger, &actualType, 
                            &klErr, sizeof (klErr), &actualSize);
	}

	/* Quit Kerberos Login Helper */
	err = QuitKerberosLoginHelper (&helperDesc);

	/* Cleanup memory */
	if (helperDesc.dataHandle != nil)
		AEDisposeDesc (&helperDesc);

	if (helperEvent.dataHandle != nil)
		AEDisposeDesc (&helperEvent);
		
	if (replyDesc.dataHandle != nil)
		AEDisposeDesc (&replyDesc);

	if (replyEvent.dataHandle != nil)
		AEDisposeDesc (&replyEvent);
			
	if (err != noErr)
		return err;
	else
		return klErr;
}

KLStatus
KLHCancelAllDialogs ()
{
	AppleEvent 	helperEvent = {typeNull, nil};
	AppleEvent 	replyEvent = {typeNull, nil};
	AEDesc 		replyDesc = {typeNull, nil};
	AEDesc 		helperDesc = {typeNull, nil};
	
	OSStatus 	err = noErr;
	KLStatus	klErr = klNoErr;

	/* Launch the Kerberos Login Helper and get a AE descriptor for it */	
	err = LaunchKerberosLoginHelper (&helperDesc);

	if (err == noErr) {
		err = AECreateAppleEvent (kKLHEventClass, kAECancelAllDialogs, &helperDesc, kAutoGenerateReturnID, kAnyTransactionID, &helperEvent);
	}
	
#if DCON
	dprintae (&helperEvent);
#endif

	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESendWorkaround (&helperEvent, &replyEvent, kAEWaitReply | kAECanInteract, kAENormalPriority, 
						kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf("Sending a kAECancelAllDialogs event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (err == noErr) {
		DescType	actualType;
		Size		actualSize;
		
		/* Check whether we have an error reply or not */
		err = AEGetParamPtr (&replyEvent, keyKLError, typeLongInteger, &actualType, 
                                &klErr, sizeof (klErr), &actualSize);
	}

	/* Quit Kerberos Login Helper */
	err = QuitKerberosLoginHelper (&helperDesc);

	/* Cleanup memory */
	if (helperDesc.dataHandle != nil)
		AEDisposeDesc (&helperDesc);

	if (helperEvent.dataHandle != nil)
		AEDisposeDesc (&helperEvent);
		
	if (replyDesc.dataHandle != nil)
		AEDisposeDesc (&replyDesc);

	if (replyEvent.dataHandle != nil)
		AEDisposeDesc (&replyEvent);
			
	if (err != noErr)
		return err;
	else
		return klErr;
}

krb5_error_code KLHPrompter (
			krb5_context	context,
			void 			*data,
	const	char			*name,
	const	char			*banner,
			int				num_prompts,
			krb5_prompt		prompts[])
{
#pragma unused(context)
#pragma unused(data)

	AppleEvent 	helperEvent = {typeNull, nil};
	AppleEvent 	replyEvent = {typeNull, nil};
	AEDesc 		replyDesc = {typeNull, nil};
	AEDesc 		helperDesc = {typeNull, nil};
	UInt32	 	i;
	OSStatus 	err = noErr;
	KLStatus	klErr = klNoErr;

	if (gKerberosLoginDialogExists) {
		// There is already a login dialog on screen somewhere
		return klDialogAlreadyExistsErr;
	}

	/* Launch the Kerberos Login Helper and get a AE descriptor for it */	
	err = LaunchKerberosLoginHelper (&helperDesc);

	if (err == noErr) {
		err = AECreateAppleEvent (kKLHEventClass, kAEPrompter, &helperDesc, 
									kAutoGenerateReturnID, kAnyTransactionID, &helperEvent);
	}
	
	/* We ignore the context and data arguments for now.  
	   The context is @#$&%*#!! hard to flatten and the data isn't used */
	
	/* Add the name argument */
	if ((err == noErr) && (name != NULL)) {
		err = AEPutParamPtr (&helperEvent, keyKLPrompterName, typeKLPrompterName, 
								name, (Size) (strlen (name) + 1));
	}

	/* Add the banner argument */
	if ((err == noErr) && (banner != NULL)) {
		err = AEPutParamPtr (&helperEvent, keyKLPrompterBanner, typeKLPrompterBanner, 
							banner, (Size) (strlen (banner) + 1));
	}
	
	/* Add the num_prompts argument */
	if (err == noErr) {
		err = AEPutParamPtr (&helperEvent, keyKLPrompterNumPrompts, typeKLPrompterNumPrompts, 
							&num_prompts, sizeof (num_prompts));
	}

	/* Add the prompter strings argument */
	if (err == noErr) {
		UInt32 	 promptSize = 0;
		char	*promptStrings = NULL;
		char	*currentPrompt;
		
		for (i = 0; i < num_prompts; i++) {
			promptSize += strlen (prompts[i].prompt) + 1;
		}
		
		promptStrings = (char *) malloc (promptSize * sizeof(char));
		if (promptStrings == NULL) 
			err = memFullErr;
		
		if (err == noErr) {
			currentPrompt = promptStrings;
			for (i = 0; i < num_prompts; i++) {
				memcpy (currentPrompt, prompts[i].prompt, strlen (prompts[i].prompt) + 1);
				currentPrompt += strlen (prompts[i].prompt) + 1;
			}

			err = AEPutParamPtr (&helperEvent, keyKLPrompterPromptStrings, typeKLPrompterStrings, 
								promptStrings, (Size) promptSize);
								
			free (promptStrings);
		}
	}
	
	/* Add the hidden flags */
	if (err == noErr) {
		std::string	promptBooleans;
		
		for (i = 0; i < num_prompts; i++) {
			promptBooleans += prompts[i].hidden ? '1' : '0';
		}

		err = AEPutParamPtr (&helperEvent, keyKLPrompterPromptHidden, typeKLPrompterBooleans, 
							promptBooleans.c_str(), (Size) (promptBooleans.length() + 1));
	}

	/* Add the max sizes of the replies. Replies are allocated for the prompter by krb5.  
	   We need to know what to allocate for the prompter. */
	if (err == noErr) {
		int		*promptReplyMaxSizes = (int *) malloc (num_prompts * sizeof (int));
		
		if (promptReplyMaxSizes == NULL)
			err = memFullErr;
		
		if (err == noErr) {
			for (i = 0; i < num_prompts; i++) {
				memcpy (&promptReplyMaxSizes[i], &prompts[i].reply->length, sizeof (prompts[i].reply->length));
			}

			err = AEPutParamPtr (&helperEvent, keyKLPrompterReplyMaxSizes, typeKLPrompterMaxSizes, 
								promptReplyMaxSizes, (Size) (num_prompts * sizeof (int)));
								
			free (promptReplyMaxSizes);
		}
	}

	
#if DCON
	dprintae (&helperEvent);
#endif

	if (err == noErr) {
		if (::RunningUnderClassic () && (KLHIdleHaveEventFilter () == false)) {
			gIdleCursor = new KLAppearanceIdleCursor;
		}
		err = AESendWorkaround (&helperEvent, &replyEvent, kAEWaitReply | kAECanInteract, kAENormalPriority, 
						kNoTimeOut, GetKLHIdleCallbackUPP (), nil);
		dprintf("Sending a kAEAcquireNewInitialTickets event to Kerberos Login Helper returned '%ld'\n", err);
		if (gIdleCursor != NULL) {
			delete gIdleCursor;
			gIdleCursor = NULL;
		}
	}
	
	if (err == noErr) {
		DescType	actualType;
		Size		actualSize;
		
		/* Check whether we have an error reply or not */
		err = AEGetParamPtr (&replyEvent, keyKLError, typeLongInteger, &actualType, 
                            &klErr, sizeof (klErr), &actualSize);
		if (err != noErr) {
			/* KLPrompter returned noErr */
			/* extract the replies */
			
			Size 		prompterRepliesSize = 0;
			DescType	prompterReplyType;
			Handle		prompterReplyHandle = nil;
			
			err = AESizeOfParam (&replyEvent, keyKLPrincipal, &prompterReplyType, &prompterRepliesSize);
			
			prompterReplyHandle = NewHandle (prompterRepliesSize);
			if (prompterReplyHandle == nil ) {
				err = klMemFullErr;
			}
			
			if (err == noErr) {
				HLock (prompterReplyHandle);
				err = AEGetParamPtr (&replyEvent, keyKLPrompterReplies, prompterReplyType, &actualType, 
									*prompterReplyHandle, prompterRepliesSize, &actualSize);
			}
			
			/* The replies are just a bunch of strings appended to each other.
			   The dialog is guaranteed to only return valid c strings to us.  */
			if (err == noErr) {
				UInt32	 i;
				char	*currentReply = *prompterReplyHandle;

				for (i = 0; i < num_prompts; i++) {
					UInt32	replyLength = strlen (currentReply);
				
					Assert_ (replyLength < prompts[i].reply->length); 
					memcpy (prompts[i].reply->data, currentReply, replyLength + 1);
					prompts[i].reply->length = (Size) replyLength;
					
					currentReply += replyLength + 1;
				}
			}
		}
	}
	
	/* Quit Kerberos Login Helper */
	err = QuitKerberosLoginHelper (&helperDesc);

	/* Cleanup memory */
	if (helperDesc.dataHandle != nil)
		AEDisposeDesc (&helperDesc);

	if (helperEvent.dataHandle != nil)
		AEDisposeDesc (&helperEvent);
		
	if (replyDesc.dataHandle != nil)
		AEDisposeDesc (&replyDesc);

	if (replyEvent.dataHandle != nil)
		AEDisposeDesc (&replyEvent);

	if (err != noErr)
		return err;
	else
		return klErr;
}

		