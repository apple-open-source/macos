/*
 * CCIException.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/Exception.h,v 1.2 2003/03/17 20:48:23 lxs Exp $
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