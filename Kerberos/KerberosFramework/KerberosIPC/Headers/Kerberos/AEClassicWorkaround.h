const OSType	kWorkaroundEventClass = FOUR_CHAR_CODE ('Krb ');	
const OSType	kWorkaroundEventReply = FOUR_CHAR_CODE ('Rply');	
 
pascal OSErr AESendWorkaround (
	const AppleEvent*		inAppleEvent,
	AppleEvent*				outReply,
	AESendMode				inSendMode,
	AESendPriority			inSendPriority,
	SInt32					inTimeout,
	AEIdleUPP				inIdleProc,
	AEFilterUPP				inFilterProc);
    

pascal OSErr ClassicReplyWorkaround	(
	const AppleEvent *inRequest,
	AppleEvent *outReply,
	long inReference);

