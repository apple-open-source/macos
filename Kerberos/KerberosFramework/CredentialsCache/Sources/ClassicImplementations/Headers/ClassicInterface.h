#pragma once

#include "HandleBuffer.h"

#define CatchForClassicReturn_(result)				\
	catch (CCIException& e) {				\
		result = e.Error ();				\
                dprintf ("Caught %d\n", result);		\
	} catch (...) {						\
		CCISignal_ ("Uncaught exception");		\
		result = ccErrBadParam;				\
	}

class CCIClassicInterface {
    public:

	CCIClassicInterface (
		CCIUInt32			inEventID,
		const AppleEvent*	inEvent,
		AppleEvent*			outReply);
                
        virtual ~CCIClassicInterface ();

	virtual void HandleEvent () = 0;
        
        CCIHandleBuffer		mSendBuffer;
        CCIHandleBuffer		mReceiveBuffer;
	
	protected:
		CCIUInt32			mEventID;
		const AppleEvent*	mEvent;
		AppleEvent*			mReply;
                
		void AddDiffsToReply (
			CCIHandleBuffer&	ioReply,
                        CCIUInt32			inServerID,
			CCIUInt32			inSeqNo);
                        
                void AddInitialDiffsToReply (
                        CCIHandleBuffer&	ioReply,
                        CCIUInt32			inServerID);
			
		void ExtractMessage ();
		void PrepareReply ();
                void CheckServerID (
                        CCIUInt32	serverID);
};