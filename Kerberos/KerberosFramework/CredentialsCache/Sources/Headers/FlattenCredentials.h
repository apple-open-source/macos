/*
 * CCICredentials.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/FlattenCredentials.h,v 1.3 2002/02/25 05:54:45 lxs Exp $
 */
 
#pragma once

#include "Credentials.h"

// Stream I/O operators for credentials

std::ostream& operator << (std::ostream& ioStream, const cc_credentials_union& inCredentials);
std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v4_t& inCredentials);
std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v5_t& inCredentials);
std::ostream& operator << (std::ostream& ioStream, const cc_data& inData);

std::istream& operator >> (std::istream& ioStream, cc_credentials_v4_t& inCredentials);
std::istream& operator >> (std::istream& ioStream, cc_credentials_v5_t& inCredentials);
std::istream& operator >> (std::istream& ioStream, cc_data& inData);

#ifdef CCache_v2_compat
std::ostream& operator << (std::ostream& ioStream, const cred_union& inCredentials);
std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v4_compat& inCredentials);
std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v5_compat& inCredentials);

std::istream& operator >> (std::istream& ioStream, cc_credentials_v4_compat& inCredentials);
std::istream& operator >> (std::istream& ioStream, cc_credentials_v5_compat& inCredentials);
#endif

//#define xxx