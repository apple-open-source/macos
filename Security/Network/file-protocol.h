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
// file-protocol - FILE protocol objects
//
#ifndef _H_FILE_PROTOCOL
#define _H_FILE_PROTOCOL

#include "transfer.h"
#include "xfercore.h"
#include "protocol.h"


namespace Security {
namespace Network {


//
// The Protocol object for the file (local file access) protocol
//
class FileProtocol : public Protocol {
    class FileTransfer;
public:
    FileProtocol(Manager &mgr);
    
    FileTransfer *makeTransfer(const Target &target, Operation operation);
    
private:
    class FileTransfer : public Transfer, protected TransferEngine::Client, protected FileDesc {
    public:
        FileTransfer(FileProtocol &proto, const Target &tgt, Operation op);
        void transitError(const CssmCommonError &error);
        int fileDesc() const;
    };
    
    class Reader : public FileTransfer {
    public:
        Reader(FileProtocol &proto, const Target &tgt);
        void transit(Event event, char *input, size_t inputLength);
        
    protected:
        void start();
    };
    
    class Writer : public FileTransfer {
    public:
        Writer(FileProtocol &proto, const Target &tgt);
        void transit(Event event, char *input, size_t inputLength);
        
    protected:
        void start();
    };
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_FILE_PROTOCOL
