#ifndef KClientAddress_h_
#define KClientAddress_h_

#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

// This class represents a network adress used by KClient

class KClientAddressPriv {
	public:
		KClientAddressPriv ();
		KClientAddressPriv (
			const KClientAddress&		inAddress);
			
		operator const KClientAddress& () const;
	
		KClientAddressPriv& operator = (const KClientAddressPriv&);
		
		operator struct sockaddr_in () const;
		
		UInt32 GetAddress () const;
		UInt16 GetPort () const;
		
	private:
		KClientAddressPriv (const KClientAddressPriv&);
		
		KClientAddress	mAddress;
};

#endif /* KClientAddress_h_ */
