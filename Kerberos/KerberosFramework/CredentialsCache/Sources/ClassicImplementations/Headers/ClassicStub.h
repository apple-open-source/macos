#pragma once

/*
 * The client end of the client-server AE interface used by 
 * Classic ticket sharing
 */

#include <Processes.h>

#include "HandleBuffer.h"

class CCIClassicStub {
	public:
		CCIClassicStub ();
		
		// Reset all buffers and prepare to send message of the specified type
		void Reset (
			OSType					inMessageType);
		// Send the message to the server
		void SendMessage  ();

		// Send and receive buffers 		
		CCIHandleBuffer		mSendBuffer;
		CCIHandleBuffer		mReceiveBuffer;
		
		// Sequence number of the last message
		CCIUInt32	GetLastSeqNo () const { return sLastSeqNo; }

	private:
		// Up the message sequence number
		void IncrementSeqNo () { sLastSeqNo++; }
	
		// Send an AE message to the server
		static void SendCCacheAE (
			Ptr				inEventData,
			Size			inEventDataSize,
			CCIUInt32		inEventID,
			bool			inWait,
			Handle&			outReply,
			AEIdleUPP		inIdleProc);

		// Create an AE appropriate for sending to the server
		static void MakeAppleEvent (
			AppleEvent&		outAppleEvent);

		// Get server's AE address
		static void GetServerAddress (
			AEAddressDesc&	outServerAddress);

		// Send a fully formed AE to the server
		static void SendAEToServer (
			AppleEvent&	inAppleEvent,
			AppleEvent&	outReply,
			bool		inWait,
			AEIdleUPP	inIdleProc);
			
		// Launch the server if it's not running
		static void LaunchYellowServer ();
			
		// Validate the PSN of the server (to see if it's been relaunched or killed)
		static bool ServerPSNIsValid ();
		
		// Extract server's reply message from the AE reply
		static void ExtractReplyMessage (
			const	AppleEvent&	inReplyEvent,
					Handle&		outReplyMessage);

		// Apply diffs from server's reply to the Classic cache
		bool ApplyCCacheDifferences ();
		// Apple one diff from a series to the Classic cache
		bool ApplyOneCCacheDifference ();
		// Initialize the diff handling data structures so that we can receive diffs from 
		// the yellow server
		void InitializeDiffs ();
		// Reset the ccache to blank state (when the server is restarted)
		void ResetCCache ();

		// Server PSN			
		static ProcessSerialNumber	sServerPSN;
		// Have the diff handling data structures been initialized?
		static bool					sDiffsHaveBeenInitialized;
		// Sequence number of the last diff we applied to the classic cache
		static CCIUInt32			sLastSeqNo;
		// ID (BSD pid) of the server, used to detect when the server is restarted.
		static CCIUInt32			sServerID;
		
		// The ID of the next event to be sent out by this stub
		OSType					mEventID;

		// Internal API functions which interface to the Classic client/server layer		
		friend cc_int32 __CredentialsCacheInternalInitiateSyncWithYellowCache (void);
		friend cc_int32 __CredentialsCacheInternalCompleteSyncWithYellowCache (
			const	AppleEvent*		inAppleEvent);
		friend cc_int32 __CredentialsCacheInternalSyncWithYellowCache (
			AEIdleUPP		inIdleProc);
};