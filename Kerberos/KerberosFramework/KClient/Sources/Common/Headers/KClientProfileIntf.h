#pragma once

class KClientProfileInterface {
public:

		KClientProfileInterface ();
		KClientProfileInterface (
			profile_t			inProfileHandle);
		~KClientProfileInterface ();
		
#ifdef KClientDeprecated_
		
	void
		GetLocalRealm (
					char*		outRealm) const;
			
	void
		SetLocalRealm (
			const 	char*		inRealm);
			
	void
		GetRealmOfHost (
			const	char*		inHost,
					char*		outRealm) const;
					
	void
		AddRealmMap (
			const	char*		inDomain,
			const	char*		inRealm);
			
	void
		DeleteRealmMap (
			const	char*		inHost);
	
	void
		GetNthRealmMap (
					SInt32		inIndex,
					char*		outHost,
					char*		outRealm) const;
	
	void
		GetNthServer (
					SInt32		inIndex,
			const	char*		inRealm,
					Boolean		inAdmin,
					char*		outHost) const;
					
	void
		AddServerMap (
			const	char*		inHost,
			const	char*		inRealm,
					Boolean		inAdmin);
					
	void
		DeleteServerMap (
			const	char*		inRealm,
			const	char*		inHost);
			
	void
		GetNthServerMap (
					SInt32		inIndex,
					char*		outHost,
					char*		outRealm,
					Boolean&	outAdmin) const;

	UInt16
		GetNthServerPort (
					SInt32		inIndex) const;

	void
		SetNthServerPort (
					SInt32		inIndex,
					UInt16		inPort);

#endif // KClientDeprecated_

private:
	UProfile		mProfile;

									KClientProfileInterface (
										KClientProfileInterface&	inOriginal);
		KClientProfileInterface&	operator = (
										KClientProfileInterface&	inOriginal);

};