/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// inetreply - manage Internet-standard reply strings
//
// The InetReply class represents an Internet-standard reply line of the form
//	nnn some text
//
#ifndef _H_INETREPLY
#define _H_INETREPLY

#include <security_utilities/utilities.h>
#include <cstdarg>


namespace Security {
namespace IPPlusPlus {


//
// An InetReply object represents a broken-up reply line of the form
//	nnn(sp)text-form
// Note that this will take a *writable* input line buffer and munge it
// into shape. This means that
//  (a) You have to keep the input line buffer alive until the InetReply dies, and
//  (b) You can't use the input line buffer after the InetReply is constructed.
//
class InetReply {
public:
    InetReply(const char *buffer);
    
    bool valid() const			{ return mCode >= 0; }
    unsigned int code() const	{ return mCode; }
    operator unsigned int () const { return code(); }
    unsigned int type() const	{ return mCode / 100; }
    const char *message() const	{ return mMessage; }
    bool isContinued() const	{ return mSeparator == '-'; }
    
private:
    const char *mBuffer;		// base buffer
    int mCode;					// integer code (usually nnn)
    char mSeparator;			// character after code (usually space; '-' for continued lines)
    const char *mMessage;		// rest of message
    
    void analyze();
    
public:
    //
    // Handle FTP-style continuations: nnn- ... nnn<sp>Message
    // Instructions for use:
    //	Continuation myCont;	// in some persistent place
    //	... get a line of reply -> const char *input ...
    //	if (myCont(input)) /* in (old) continuation */;
    //	InetReply reply(input);
    //	if (myCont(reply)) /* in (newly started) continuation */;
    //	/* not (any more) in continuation; reply has last message line
    //
    class Continuation {
    public:
        Continuation() : mActive(false) { }
        
        bool operator () (const char *input);
        bool operator () (const InetReply &reply);
        
        bool active() const	{ return mActive; }
        
    private:
        bool mActive;
        char mTestString[4];
    };
};


}	// end namespace IPPlusPlus
}	// end namespace Security


#endif //_H_INETREPLY
