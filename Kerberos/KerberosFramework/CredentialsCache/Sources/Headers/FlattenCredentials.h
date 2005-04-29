/*
 * CCICredentials.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/FlattenCredentials.h,v 1.4 2004/09/08 20:48:35 lxs Exp $
 */
 
#pragma once

#include "Credentials.h"

// Stream I/O operators for credentials
void WriteCredentials (std::ostream& ioStream, const cc_credentials_union& inCredentials);

void ReadV4Credentials (std::istream& ioStream, cc_credentials_v4_t& inCredentials);
void WriteV4Credentials (std::ostream& ioStream, const cc_credentials_v4_t& inCredentials);

void ReadV5Credentials (std::istream& ioStream, cc_credentials_v5_t& inCredentials);
void WriteV5Credentials (std::ostream& ioStream, const cc_credentials_v5_t& inCredentials);

#ifdef CCache_v2_compat
void WriteCompatCredentials (std::ostream& ioStream, const cred_union& inCredentials);

void ReadV4CompatCredentials (std::istream& ioStream, cc_credentials_v4_compat& inCredentials);
void WriteV4CompatCredentials (std::ostream& ioStream, const cc_credentials_v4_compat& inCredentials);

void ReadV5CompatCredentials (std::istream& ioStream, cc_credentials_v5_compat& inCredentials);
void WriteV5CompatCredentials (std::ostream& ioStream, const cc_credentials_v5_compat& inCredentials);
#endif

void ReadData (std::istream& ioStream, cc_data& inData);
void WriteData (std::ostream& ioStream, const cc_data& inData);

void ReadDataArray (std::istream& ioStream, cc_data**& inArray);
void WriteDataArray (std::ostream& ioStream, cc_data** inArray);

void ReadString (std::istream& ioStream, char *&outString);
void WriteString (std::ostream& ioStream, const char *inString);

void ReadString (std::istream& ioStream, std::string &outString);
void WriteString (std::ostream& ioStream, const std::string inString);

void ReadUInt32 (std::istream& ioStream, CCIUInt32& integer);
void WriteUInt32 (std::ostream& ioStream, CCIUInt32 integer);
