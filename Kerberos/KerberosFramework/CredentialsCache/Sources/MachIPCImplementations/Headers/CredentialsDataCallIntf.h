#pragma once

class CCICredentialsDataCallInterface {
	public:
		CCICredentialsDataCallInterface (
			const CCIUniqueID&		inCredentials);
			
		CCICredentialsData* operator -> () { return mCredentials; }
		
	private:
		CCICredentialsData*	mCredentials;
};

