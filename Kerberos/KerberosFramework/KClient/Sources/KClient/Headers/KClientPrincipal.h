#pragma once

class StKClientPrincipal {
	public:
		StKClientPrincipal (
			KClientPrincipal		inPrincipal):
			mPrincipal (reinterpret_cast <UPrincipal*> (inPrincipal)) {}
		
		StKClientPrincipal (
			UPrincipal*				inPrincipal):
			mPrincipal (inPrincipal) {}
		
		operator UPrincipal& () { return *mPrincipal; }
		
		UPrincipal*	operator -> () { return mPrincipal; }
		
		operator KClientPrincipal () { return reinterpret_cast <KClientPrincipal> (mPrincipal); }
	private:
		UPrincipal*		mPrincipal;
};
