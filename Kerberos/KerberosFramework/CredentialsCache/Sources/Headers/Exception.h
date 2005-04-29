/*
 * CCIException.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/Exception.h,v 1.3 2004/10/22 20:48:29 lxs Exp $
 */
 
#pragma once

class CCIException {
	public:
		CCIException	(CCIResult	inError):
			mError (inError) {}
		
		CCIResult	Error () const { return mError; }
	
	private:
		CCIResult		mError;
};
