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


/*  *********************************************************************
    File: ssltrspt.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssltrspt.h   SSL Transport Layer

    A single fabulous prototype for the single function in ssltrspt.h
    which isn't in ssl.h. You have found the SSLRef easter egg (such as
    it is). SSLRef by Tim Dierks, with help from Eric Gundrum, Chris
    Allen, Jonathan Zamick, and Michael Rutman. Thanks also to Jim
    CastroLang, Clare Burmeister, and Tony Hughes. Also, thanks to our
    friends at Netscape: Tom Weinstein, Jeff Weinstein, Phil Karlton, and
    Eric Greenberg.

    ****************************************************************** */

#ifndef _SSLTRSPT_H_
#define _SSLTRSPT_H_ 1

#ifndef _SSL_H_
#include "ssl.h"
#endif

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

SSLErr QueueMessage(SSLRecord rec, SSLContext *ctx);

#endif
