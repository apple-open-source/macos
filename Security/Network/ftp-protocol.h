/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// ftp-protocol - FTP protocol objects
//
#ifndef _H_FTP_PROTOCOL
#define _H_FTP_PROTOCOL

#include "xfercore.h"
#include "protocol.h"
#include "transfer.h"
#include "netconnection.h"
#include "neterror.h"
#include <Security/ip++.h>
#include <Security/inetreply.h>


namespace Security {
namespace Network {


//
// The Protocol object for the FTP protocol
//
class FTPProtocol : public Protocol {
    class FTPTransfer;
    class FTPConnection;
public:
    static const IPPort defaultFtpPort = 21;

    FTPProtocol(Manager &mgr);
    
public:
    FTPTransfer *makeTransfer(const Target &target, Operation operation);
    
public:
    // FTP-specific operation codes
    enum {
        downloadDirectory = protocolSpecific,	// get filename list (NLST)
        downloadListing,						// get host-specific listing (LIST)
        makeDirectory,							// make a directory (MKD)
        removeDirectory,						// remove a directory (RMD)
        genericCommand							// issue generic FTP command
    };
    
private:
    //
    // The data connection object manages a data pipe (for one upload/download)
    //
    class FTPDataConnection : public TransferEngine::Client, public TCPClientSocket {
    public:
        FTPDataConnection(FTPConnection &conn) : connection(conn) { }

        FTPConnection &connection;		// the main Connection we belong to

        void start(Sink &sink);			// start download
        void start(Source &source);		// start upload
        void close();					// unconditional close
        void connectionDone();			// Connection is done
        
        OSStatus status() const			{ return mFailureStatus; }
        
        int fileDesc() const;
        
    protected:
        void transit(Event event, char *input, size_t inputLength);
        void transitError(const CssmCommonError &error);
        void setup();
        void finish();
    
    private:
        OSStatus mFailureStatus;		// noErr unless something went wrong
        bool mTransferDone;				// our transfer is all done
        bool mConnectionDone;			// our Connection is ready to finish()
    }; 

    //
    // This is the persistent connection object.
    //
    class FTPConnection : public TCPConnection {
        friend class FTPDataConnection;
    public:
        FTPConnection(Protocol &proto, const HostTarget &tgt);
    
        // state machine master state
        enum State {
            errorState,				// invalid state marker (reset or fail)

            // login sub-engine
            loginInitial,			// just connected [want hello or need-login]
            loginUserSent,			// USER command sent [want hello or need-pass]
            loginPassSent,			// PASS command sent [want dispatch command]

            // idle state
            idle,		// at command prompt, idle [nothing pending]
            
            // data transfer states
            typeCommandSent,		// sent TYPE command [want ok]
            passiveSent,			// sent PASV [want contact address]
            portSent,				// sent PORT [want port ok]
            restartSent,			// sent REST [want 350 Restarting...]
            transferSent,			// sent RETR et al [want transfer starting]
            transferInProgress,		// download in progress [want transfer complete]
            
            // misc. states
            directCommandSent,		// sent non-transfer command, want success
            
            START = loginInitial
        };
        
        FTPTransfer &transfer() { return transferAs<FTPTransfer>(); }
        
        void request(const char *path);
        void abort();
        
    protected:
        void transit(Event event, char *input, size_t inputLength);
        void transitError(const CssmCommonError &error);
        bool validate();

        void startCommand();		// initiate mOperation, if any
        void startTransfer(bool restarted = false);
        
        bool imageMode() const		{ return mImageMode; }
        void imageMode(bool mode);
        
        void fail(const char *reply, OSStatus error = Transfer::defaultOSStatusError)
        { setError(reply, error); Error::throwMe(error); }
        void fail()		{ retain(false); Connection::fail(); }

    protected:
        State state;				// state engine state
        InetReply::Continuation replyContinuation; // cotinued-reply marker

        // state describing the ongoing connection
        bool mImageMode;			// in image (vs. ascii) mode
        bool mPassive;				// current transfer is in passive mode
        
        string mOperationPath;		// remote path for operation
        
        FTPDataConnection mDataPath; // subsidiary (data transfer) connection
        TCPServerSocket mReceiver;	// incoming listen socket for active mode transfers
    };
    
    //
    //  The official Transfer object (for all kinds of transfers)
    //
    class FTPTransfer : public Transfer {
    public:
        FTPTransfer(Protocol &proto, const Target &target, Operation operation);
        
        ResultClass resultClass() const;
        
    protected:
        void start();					// start me up
        void abort();					// abort this Transfer
        
        string mFailedReply;			// reply string that triggered failure
    };
    
private:
    struct FTPAddress {
        unsigned int h1, h2, h3, h4, p1, p2;
        
        FTPAddress() { }
        FTPAddress(const IPSockAddress &addr);
        operator IPSockAddress () const;
    };
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_FTP_PROTOCOL
