/*
 * KClient bottom layer to Login library
 *
 * $Header: /cvs/kfm/KerberosFramework/KClient/Sources/Common/KClientLoginIntf.cp,v 1.22 2003/02/26 04:09:10 lxs Exp $
 */

#include "KClientLoginIntf.h"

std::string
KClientLoginInterface::AcquireInitialTickets (
	const UPrincipal&	inPrincipal)
{
	std::string		result;
	KLPrincipal		newPrincipal;
	KLPrincipal		oldPrincipal;
	char*			ccacheName;

	OSStatus err;
	if (inPrincipal.Get () != nil) {
		try {
			oldPrincipal = inPrincipal.GetKLPrincipal ();
		} catch (UPrincipalRuntimeError& e) {
			if (e.Error () == klPreferencesReadErr) {
				throw KClientError (kcErrInvalidPreferences);
			} else {
				throw;
			}
		}
		err = ::KLAcquireInitialTickets (oldPrincipal, nil, &newPrincipal, &ccacheName);
		::KLDisposePrincipal (oldPrincipal);
	} else {
		err = ::KLAcquireInitialTickets (nil, nil, &newPrincipal, &ccacheName);
	}
		
	if (err != noErr)
		DebugThrow_ (KClientLoginLibRuntimeError (err));
		
	result = ccacheName;
	KLDisposeString (ccacheName);
	KLDisposePrincipal (newPrincipal);
	return result;
}

std::string
KClientLoginInterface::AcquireInitialTicketsWithPassword (
	const UPrincipal&	inPrincipal,
	const char*			inPassword)
{
	std::string		result;

	char*			ccacheName;
	OSStatus		err;
	
	err = ::KLAcquireInitialTicketsWithPassword (inPrincipal.GetKLPrincipal (), nil, inPassword, &ccacheName);
	if (err != noErr)
		DebugThrow_ (KClientLoginLibRuntimeError (err));
		
	result = ccacheName;
	KLDisposeString (ccacheName);
	return result;
}

void
KClientLoginInterface::Logout (
	const UPrincipal&	inPrincipal)
{
	KLPrincipal		principal = nil;
	
	if (inPrincipal.Get () != nil)
		principal = inPrincipal.GetKLPrincipal ();

	OSStatus err = KLDestroyTickets (principal);
	if (principal != nil)
		KLDisposePrincipal (principal);

	if (err != noErr) {
		DebugThrow_ (KClientLoginLibRuntimeError (err));
	}
}

void
KClientLoginInterface::GetTicketExpiration (
	const UPrincipal&	inPrincipal,
	UInt32&				outExpiration)
{
	KLPrincipal		principal = nil;
	
	if (inPrincipal.Get () != nil)
		principal = inPrincipal.GetKLPrincipal ();

	OSStatus err = KLTicketExpirationTime (principal, kerberosVersion_All, (KLTime *)&outExpiration);
	if (principal != nil)
		KLDisposePrincipal (principal);

	if (err != noErr) {
		DebugThrow_ (KClientLoginLibRuntimeError (err));
	}
}

UInt32
KClientLoginInterface::GetDefaultTicketLifetime ()
{
	KLLifetime	lifetime;
	KLSize	size = sizeof (lifetime);

	OSStatus err = KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime, &lifetime, &size);
	AssertReturnValue_ (err == noErr);
	return lifetime;
}
