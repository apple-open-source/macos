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
// file-protocol - File protocol objects
//
#include "file-protocol.h"
#include "netmanager.h"
#include "neterror.h"
#include "netparameters.h"


namespace Security {
namespace Network {


//
// Construct the protocol object
//
FileProtocol::FileProtocol(Manager &mgr) : Protocol(mgr, "file")
{
}


//
// Create a Transfer object for our protocol
//
FileProtocol::FileTransfer *FileProtocol::makeTransfer(const Target &target, Operation operation)
{
    switch (operation) {
    case download:
        return new Reader(*this, target);
        break;
    case upload:
        return new Writer(*this, target);
    default:
        Error::throwMe();
    }
}


FileProtocol::FileTransfer::FileTransfer(FileProtocol &proto, const Target &tgt, Operation op)
    : Transfer(proto, tgt, op)
{
}

int FileProtocol::FileTransfer::fileDesc() const
{ return *this; }


void FileProtocol::FileTransfer::transitError(const CssmCommonError &error)
{
    fail();
}


//
// Read transfers
//
FileProtocol::Reader::Reader(FileProtocol &proto, const Target &tgt) 
    : FileTransfer(proto, tgt, download)
{
}

void FileProtocol::Reader::start()
{
    open(target.path.c_str());

     // notify any observer that we are under way.
     observe(Observer::resourceFound);
    observe(Observer::downloading);
    
	setFlag(O_NONBLOCK);
    int restartOffset = getv<int>(kNetworkRestartPosition, 0);
    if (restartOffset)
        seek(restartOffset);
    size_t size = fileSize() - restartOffset;
    mode(sink(), size);
    sink().setSize(size);
    protocol.manager.addIO(this);
}

void FileProtocol::Reader::transit(Event event, char *, size_t)
{
    assert(event == autoReadDone);
    protocol.manager.removeIO(this);
    finish();
}


//
// Write transfers
//
FileProtocol::Writer::Writer(FileProtocol &proto, const Target &tgt)
    : FileTransfer(proto, tgt, upload)
{
}

void FileProtocol::Writer::start()
{
    open(target.path.c_str(), O_WRONLY | O_CREAT);
    
    // notify any observer that we are under way.
    observe(Observer::resourceFound);
    observe(Observer::uploading);
    
    int restartOffset = getv<int>(kNetworkRestartPosition, 0);
    if (restartOffset)
        seek(restartOffset);
    protocol.manager.addIO(this);
    disable(input);
    mode(source(), fileSize() - restartOffset);
}

void FileProtocol::Writer::transit(Event event, char *, size_t)
{
    assert(event == autoWriteDone);
    protocol.manager.removeIO(this);
    finish();
}


}	// end namespace Network
}	// end namespace Security
