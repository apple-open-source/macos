/*
 * Bottom level interface from KClient to Login library
 */

#pragma once

class KClientLoginInterface {
	public: 
		static	std::string				AcquireInitialTickets (
											const UPrincipal&			inPrincipal);

		static	std::string				AcquireInitialTicketsWithPassword (
											const UPrincipal&			inPrincipal,
											const char*					inPassword);

		static	void					Logout (
											const UPrincipal&	inPrincipal);

		static	void					GetTicketExpiration (
											const UPrincipal&			inPrincipal,
											UInt32&						outExpiration);
		static	UInt32					GetDefaultTicketLifetime ();
};
