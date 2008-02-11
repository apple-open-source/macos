/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// reqinterp - Requirement language (exprOp) interpreter
//
#ifndef _H_REQINTERP
#define _H_REQINTERP

#include <security_codesigning/reqreader.h>
#include <Security/SecTrustSettings.h>

namespace Security {
namespace CodeSigning {


//
// An interpreter for exprForm-type requirements.
// This is a simple Polish Notation stack evaluator.
//	
class Requirement::Interpreter : public Requirement::Reader {	
public:
	Interpreter(const Requirement *req, const Context *ctx);
	
	bool evaluate();
	
protected:
	class Match {
	public:
		Match(Interpreter &interp);		// reads match postfix from interp
		Match(CFStringRef value, MatchOperation op) : mValue(value), mOp(op) { } // explicit
		bool operator () (CFTypeRef candidate) const; // match to candidate
		
	private:
		CFCopyRef<CFStringRef> mValue;	// match value
		MatchOperation mOp;				// type of match
	};
	
protected:
	bool infoKeyValue(const std::string &key, const Match &match);
	bool certFieldValue(const string &key, const Match &match, SecCertificateRef cert);
	bool verifyAnchor(SecCertificateRef cert, const unsigned char *digest);
	bool appleSigned();
	bool trustedCerts();
	bool trustedCert(int slot);
	
	SecTrustSettingsResult trustSetting(SecCertificateRef cert, bool isAnchor);
	
private:
	const Context * const mContext;
};


}	// CodeSigning
}	// Security

#endif //_H_REQINTERP
