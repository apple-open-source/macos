/*
 * CCICredentialsDataClassicStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/CredsDataClassicStubs.cp,v 1.4 2003/03/17 20:48:07 lxs Exp $
 */

#include "CredsDataClassicStubs.h"
#include "ClassicProtocol.h"
#include "FlattenCredentials.h"

CCICredentialsDataClassicStub::CCICredentialsDataClassicStub (
	CCIUniqueID	inCredentials,
	CCIInt32	inAPIVersion,
	bool		inInitialize):
	CCICredentialsDataCallStub (inCredentials, inAPIVersion) {
}

CCICredentialsDataClassicStub::~CCICredentialsDataClassicStub () {
}

