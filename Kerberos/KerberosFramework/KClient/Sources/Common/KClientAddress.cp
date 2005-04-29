#include "KClientAddress.h"

KClientAddressPriv::KClientAddressPriv ()
{
    mAddress.port = 0;
    mAddress.address = 0;
}

KClientAddressPriv::KClientAddressPriv (
	const KClientAddress&		inAddress):
	mAddress (inAddress)
{
}

KClientAddressPriv::operator const KClientAddress& () const
{
	return mAddress;
}

KClientAddressPriv&
KClientAddressPriv::operator = (
	const KClientAddressPriv&	inOriginal)
{
	if (&inOriginal != this) {
		mAddress = inOriginal.mAddress;
	}
	
	return *this;
}

UInt32
KClientAddressPriv::GetAddress () const
{
	return mAddress.address;
}

UInt16
KClientAddressPriv::GetPort () const
{
	return mAddress.port;
}

KClientAddressPriv::operator sockaddr_in () const
{
	struct sockaddr_in addr = {0};
    addr.sin_len = sizeof (addr);
    addr.sin_family = AF_INET;
    addr.sin_port = mAddress.port;
    addr.sin_addr.s_addr = mAddress.address;
	return addr;
}
