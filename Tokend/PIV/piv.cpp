/*
 *  Copyright (c) 2004-2007 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 * piv.cpp - PIV.tokend main program
 */

#include "PIVToken.h"

/*
	The call to SecKeychainSetServerMode is used to avoid recursion when
	doing keychain calls. The only place this is relevant is when we are
	setting the print name for the token using the common name from the
	certificate. Calling this will prevent any keychain-type calls from
	working but will still allow use of SecCertificate calls, etc.
	If the header is not available, you can safely undef _USECERTIFICATECOMMONNAME
*/

#ifdef _USECERTIFICATECOMMONNAME
#include <Security/SecKeychainPriv.h>
#endif	/* _USECERTIFICATECOMMONNAME */

int main(int argc, const char *argv[])
{
#ifdef _USECERTIFICATECOMMONNAME
	SecKeychainSetServerMode();
#endif	/* _USECERTIFICATECOMMONNAME */
	secdebug("PIV.tokend", "main starting with %d arguments", argc);
	secdelay("/tmp/delay/PIV");

	token = new PIVToken();
	try {
		int ret = SecTokendMain(argc, argv, token->callbacks(), token->support());
		delete token;
		return ret;
	} catch(...) {
		delete token;
		return -1;
}
}
