#include "AEClassicWorkaround.h"
#if TARGET_RT_MAC_MACHO
typedef SInt32 RefConType;
#else
typedef UInt32 RefConType;
#endif

static pascal OSErr DoReply (
	const AppleEvent *inRequest, AppleEvent *outReply, RefConType inReference);

/*
 * As far as we can tell, there is a problem on Mac OS X 10.1.x whereby a 
 * classic application sending an AppleEvent to a native app and waiting for
 * the reply never receives the reply under certain circumstances. We don't
 * know what circumstances, and we don't know if this is a bug in our code or
 * in Mac OS X. In an attempt to work around this, we are changing our event handlers
 * to send the response back with AESend rather than returning it from the handler.
 */
 
pascal OSErr
ClassicReplyWorkaround (
	const AppleEvent*	inRequest,
	AppleEvent*			outReply,
	long inReference)
{
	AEDesc		senderDesc = {typeNull, nil};
	
	OSErr handlerErr = AEGetAttributeDesc (inRequest, keyOriginalAddressAttr, typeWildCard, &senderDesc);
	
	AppleEvent	replyEvent = {typeNull, nil};
	if (handlerErr == noErr) {
		handlerErr = AECreateAppleEvent (kWorkaroundEventClass, kWorkaroundEventReply, &senderDesc, 
			kAutoGenerateReturnID, kAnyTransactionID, &replyEvent);
	}
	
	if (handlerErr == noErr) {
		handlerErr = InvokeAEEventHandlerUPP (inRequest, &replyEvent, inReference, (AEEventHandlerUPP) inReference);
	}
	
	AppleEvent		replyReply = {typeNull, nil};
	if (handlerErr == noErr) {
		handlerErr = AESend (&replyEvent, &replyReply, kAENoReply | kAECanSwitchLayer | kAEAlwaysInteract, 
			kAEHighPriority, kAEDefaultTimeout, nil, nil);
	}
	
	if (senderDesc.dataHandle != nil) {
		AEDisposeDesc (&senderDesc);
	}
	
	if (replyEvent.dataHandle != nil) {
		AEDisposeDesc (&replyEvent);
	}
	
	if (replyReply.dataHandle != nil) {
		AEDisposeDesc (&replyReply);
	}
	
	return handlerErr;
}

pascal OSErr AESendWorkaround (
	const AppleEvent*		inAppleEvent,
	AppleEvent*				outReply,
	AESendMode				inSendMode,
	AESendPriority			inSendPriority,
	SInt32					inTimeout,
	AEIdleUPP				inIdleProc,
	AEFilterUPP				inFilterProc)
{
	AppleEvent		sendReply = {typeNull, nil};
	OSErr err = AESend (inAppleEvent, &sendReply, kAENoReply | kAEAlwaysInteract | kAECanSwitchLayer,
		kAEHighPriority, kAEDefaultTimeout, nil, nil);
		
	// Check event send mode
	if (((inSendMode & kAEWaitReply) == kAENoReply) || ((inSendMode & kAEWaitReply) == kAEQueueReply)) {
		return err;
	}
		
	static AEEventHandlerUPP	doReplyUPP = nil;
	if (doReplyUPP == nil) {
		doReplyUPP = NewAEEventHandlerUPP (DoReply);
	}
	
	AEEventHandlerUPP		oldHandler = nil;
	SInt32					oldHandlerRefcon;
	
	if (err == noErr) {
		AEGetEventHandler (kWorkaroundEventClass, kWorkaroundEventReply, &oldHandler, &oldHandlerRefcon, false);
		err = AEInstallEventHandler (kWorkaroundEventClass, kWorkaroundEventReply, doReplyUPP, (SInt32) outReply, false);
	}
	
	if (err == noErr) {
		// Accept only AppleEvents if we have no callback
		EventMask		mask = highLevelEventMask;
//		if (inIdleProc != nil) {
			mask = everyEvent;
//		}

		EventRecord		event;
		SInt32			delay = 1;
		RgnHandle		region = nil;
		Boolean			done = false;
		
		RgnHandle		updateRegion = NewRgn ();

		do {
			Boolean handled = false;
			WaitNextEvent (mask, &event, delay, region);
			switch (event.what) {
				case kHighLevelEvent:
					if ((event.message == kWorkaroundEventClass) && (*(OSType*) (&event.where) == kWorkaroundEventReply)) {
						AEProcessAppleEvent (&event);
						handled = true;
						done = true;
					}
			}
			
			if (!handled) {
				if (inIdleProc != nil) {
					InvokeAEIdleUPP (&event, &delay, &region, inIdleProc);
				} else {
					switch (event.what) {
						// Ditch unhandled update events to avoid update flood
						case updateEvt:
							GrafPtr	savePort;
							GetPort (&savePort);
							SetPortWindowPort ((WindowPtr) (event.message));
							if (updateRegion != nil) {
#if TARGET_API_MAC_CARBON
								RgnHandle	winRegion = NewRgn ();
								if (winRegion != nil) {
									::GetWindowRegion ((WindowPtr) event.message, kWindowUpdateRgn, winRegion);
									::UnionRgn (updateRegion, winRegion, updateRegion);
									::DisposeRgn (winRegion);
								}
#else
								::UnionRgn (updateRegion, ((WindowPeek) event.message) -> updateRgn, updateRegion);
#endif
							}
							::BeginUpdate ((WindowPtr) (event.message));
							::EndUpdate ((WindowPtr) (event.message));
							SetPort (savePort);
					}
				}
			}
		} while (!done);

		if (updateRegion != nil) {
			GrafPtr	savePort;
			GetPort (&savePort);

#if TARGET_API_MAC_CARBON
			WindowPtr	win = GetWindowList ();
#else
			WindowPtr	win = LMGetWindowList ();
#endif
			while (win != nil) {
#if TARGET_API_MAC_CARBON
				::SetPortWindowPort (win);
#else
				::SetPort (win);
#endif
				Point	origin = {0, 0};
				::LocalToGlobal (&origin);
				::OffsetRgn (updateRegion, -origin.h, -origin.v);
#if TARGET_API_MAC_CARBON
				::InvalWindowRgn (win, updateRegion);
#else
				::InvalRgn (updateRegion);
#endif
				::OffsetRgn (updateRegion, origin.h, origin.v);
				win = GetNextWindow (win);
			}
			
			SetPort (savePort);
		}
		
	}
	
	AERemoveEventHandler (kWorkaroundEventClass, kWorkaroundEventReply, doReplyUPP, false);

	if (sendReply.dataHandle != nil) {
		AEDisposeDesc (&sendReply);
	}
	
	if (oldHandler != nil) {
		AEInstallEventHandler (kWorkaroundEventClass, kWorkaroundEventReply, oldHandler, oldHandlerRefcon, false);
	}	
	
	return err;
}
	
pascal OSErr DoReply (
	const AppleEvent *inRequest,
	AppleEvent */* outReply */,
	RefConType inReference)
{
	AEDuplicateDesc (inRequest, (AppleEvent*) inReference);
	return noErr;
}
