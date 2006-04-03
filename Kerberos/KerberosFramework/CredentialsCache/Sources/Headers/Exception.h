/*
 * CCIException.h
 *
 * $Header$
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
