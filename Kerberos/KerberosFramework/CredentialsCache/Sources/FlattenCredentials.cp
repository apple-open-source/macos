/*
 * CCIFlattenedCredentials.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/FlattenCredentials.cp,v 1.13 2003/05/07 20:13:49 lxs Exp $
 */
 
#include "FlattenCredentials.h"

// These functions write credentials to streams and extract them back from streams.
// Used to serialize credentials structures across various IPC layers.

static void WriteDataArray (
    std::ostream&	ioStream,
    cc_data**	inArray);

static void ReadDataArray (
    std::istream&	ioStream,
    cc_data**&	inArray);

std::ostream& operator << (std::ostream& ioStream, const cc_credentials_union& inCredentials)
{
	// Output the version followed by the appropriate creds
    ioStream << inCredentials.version << std::endl;
    switch (inCredentials.version) {
        case cc_credentials_v4:
            ioStream << *inCredentials.credentials.credentials_v4 << std::endl;
            break;
        
        case cc_credentials_v5:
            ioStream << *inCredentials.credentials.credentials_v5 << std::endl;
            break;
            
        default:
            throw CCIException (ccErrBadCredentialsVersion);
    }
    return ioStream;
}

std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v4_t& inCredentials)
{
	// Just dump the creds
    for (CCIUInt32 i = 0; i < sizeof (inCredentials); i++) {
        ioStream << ((int) ((char*) (&inCredentials)) [i]) << " ";
    }
    return ioStream;
}

std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v5_t& inCredentials)
{
	// Write out various parts of the creds in order
    ioStream << inCredentials.client << std::endl;
    ioStream << inCredentials.server << std::endl;
    ioStream << inCredentials.keyblock << std::endl;
    ioStream << inCredentials.authtime << std::endl;
    ioStream << inCredentials.starttime << std::endl;
    ioStream << inCredentials.endtime << std::endl;
    ioStream << inCredentials.renew_till << std::endl;
    ioStream << inCredentials.is_skey << std::endl;
    ioStream << inCredentials.ticket_flags << std::endl;
    WriteDataArray (ioStream, inCredentials.addresses);
    ioStream << inCredentials.ticket << std::endl;
    ioStream << inCredentials.second_ticket << std::endl;
    WriteDataArray (ioStream, inCredentials.authdata);
    return ioStream;
}

std::istream& operator >> (std::istream& ioStream, cc_credentials_v4_t& inCredentials)
{
	// Suck in the creds
    for (CCIUInt32 i = 0; i < sizeof (inCredentials); i++) {
        int c;
        ioStream >> c;
        ((char*) (&inCredentials)) [i] = c;
    }
    return ioStream;
}
    
std::istream& operator >> (std::istream& ioStream, cc_credentials_v5_t& inCredentials)
{
	// Suck in the various parts of the creds in order
    try {
        std::string		newString;
        ioStream >> newString;

        inCredentials.client = new char [newString.length () + 1];
        inCredentials.client [newString.length ()] = '\0';
        newString.copy (inCredentials.client, newString.length () + 1);

        ioStream >> newString;
        inCredentials.server = new char [newString.length () + 1];
        inCredentials.server [newString.length ()] = '\0';
        newString.copy (inCredentials.server, newString.length () + 1);

        ioStream >> inCredentials.keyblock;
        ioStream >> inCredentials.authtime;
        ioStream >> inCredentials.starttime;
        ioStream >> inCredentials.endtime;
        ioStream >> inCredentials.renew_till;
        ioStream >> inCredentials.is_skey;
        ioStream >> inCredentials.ticket_flags;
        ReadDataArray (ioStream, inCredentials.addresses);
        ioStream >> inCredentials.ticket;
        ioStream >> inCredentials.second_ticket;
        ReadDataArray (ioStream, inCredentials.authdata);
    } catch (...) {
        if (inCredentials.client != NULL) {
            delete [] inCredentials.client;
            inCredentials.client = NULL;
        }
        if (inCredentials.server != NULL) {
            delete [] inCredentials.server;
            inCredentials.server = NULL;
        }
        
        throw;
    }
    return ioStream;
}

#if CCache_v2_compat

std::ostream& operator << (std::ostream& ioStream, const cred_union& inCredentials)
{
    ioStream << inCredentials.cred_type << std::endl;
    switch (inCredentials.cred_type) {
        case CC_CRED_V4:
            ioStream << *inCredentials.cred.pV4Cred << std::endl;
            break;
        
        case CC_CRED_V5:
            ioStream << *inCredentials.cred.pV5Cred << std::endl;
            break;
            
        default:
            throw CCIException (ccErrBadCredentialsVersion);
    }
    return ioStream;
}
    
std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v4_compat& inCredentials)
{
    for (CCIUInt32 i = 0; i < sizeof (inCredentials); i++) {
        ioStream << ((int) ((char*) (&inCredentials)) [i]) << " ";
    }
    return ioStream;
}

std::ostream& operator << (std::ostream& ioStream, const cc_credentials_v5_compat& inCredentials)
{
    ioStream << inCredentials.client << std::endl;
    ioStream << inCredentials.server << std::endl;
    ioStream << inCredentials.keyblock << std::endl;
    ioStream << inCredentials.authtime << std::endl;
    ioStream << inCredentials.starttime << std::endl;
    ioStream << inCredentials.endtime << std::endl;
    ioStream << inCredentials.renew_till << std::endl;
    ioStream << inCredentials.is_skey << std::endl;
    ioStream << inCredentials.ticket_flags << std::endl;
    WriteDataArray (ioStream, inCredentials.addresses);
    ioStream << inCredentials.ticket << std::endl;
    ioStream << inCredentials.second_ticket << std::endl;
    WriteDataArray (ioStream, inCredentials.authdata);
    return ioStream;
}

std::istream& operator >> (std::istream& ioStream, cc_credentials_v4_compat& inCredentials)
{
    for (CCIUInt32 i = 0; i < sizeof (inCredentials); i++) {
        int c;
        ioStream >> c;
        ((char*) (&inCredentials)) [i] = c;
    }
    return ioStream;
}
    
std::istream& operator >> (std::istream& ioStream, cc_credentials_v5_compat& inCredentials)
{
    try {
        std::string		newString;
        ioStream >> newString;

        inCredentials.client = new char [newString.length () + 1];
        inCredentials.client [newString.length ()] = '\0';
        newString.copy (inCredentials.client, newString.length () + 1);

        ioStream >> newString;
        inCredentials.server = new char [newString.length () + 1];
        inCredentials.server [newString.length ()] = '\0';
        newString.copy (inCredentials.server, newString.length () + 1);

        ioStream >> inCredentials.keyblock;
        ioStream >> inCredentials.authtime;
        ioStream >> inCredentials.starttime;
        ioStream >> inCredentials.endtime;
        ioStream >> inCredentials.renew_till;
        ioStream >> inCredentials.is_skey;
        ioStream >> inCredentials.ticket_flags;
        ReadDataArray (ioStream, inCredentials.addresses);
        ioStream >> inCredentials.ticket;
        ioStream >> inCredentials.second_ticket;
        ReadDataArray (ioStream, inCredentials.authdata);
    } catch (...) {
        if (inCredentials.client != NULL) {
            delete [] inCredentials.client;
            inCredentials.client = NULL;
        }
        if (inCredentials.server != NULL) {
            delete [] inCredentials.server;
            inCredentials.server = NULL;
        }
        
        throw;
    }
    return ioStream;
}

#endif // CCache_v2_Compat

std::ostream& operator << (std::ostream& ioStream, const cc_data& inData)
{
    ioStream << inData.type << std::endl;
    ioStream << inData.length << std::endl;
    for (CCIUInt32 i = 0; i < inData.length; i++) {
        ioStream << (int) (((char*) inData.data) [i]) << " ";
    }
    ioStream << std::endl;
    return ioStream;
}

std::istream& operator >> (std::istream& ioStream, cc_data& inData)
{
    ioStream >> inData.type;
    ioStream >> inData.length;
    inData.data = new char [inData.length];
    for (CCIUInt32 i = 0; i < inData.length; i++) {
        int c;
        ioStream >> c;
        ((char*) (inData.data)) [i] = c;
    }
    return ioStream;
}

static void WriteDataArray (
    std::ostream&	ioStream,
    cc_data**	inDataArray)
{
    CCIUInt32	arraySize = 0;
    if (inDataArray != NULL) {
        while (inDataArray [arraySize] != NULL) {
            arraySize++;
        }
    }
    
    ioStream << arraySize << std::endl;   
    
    for (CCIUInt32 i = 0; i < arraySize; i++) {
        ioStream << *(inDataArray [i]) << std::endl;
    }
}

static void ReadDataArray (
    std::istream&	ioStream,
    cc_data**&	outDataArray)
{
    CCIUInt32	arraySize = 0;
    ioStream >> arraySize;
    
    if (arraySize == 0) {
        outDataArray = NULL;
        return;
    }
    
    cc_data**	dataArray = NULL;
    try {
        dataArray = new cc_data* [arraySize + 1];
        for (CCIUInt32 i = 0; i < arraySize + 1; i++) {
            dataArray [i] = NULL;
        }
        for (CCIUInt32 i = 0; i < arraySize; i++) {
            dataArray [i] = new cc_data;
            dataArray [i] -> data = NULL;
        }
        for (CCIUInt32 i = 0; i < arraySize; i++) {
            ioStream >> *dataArray [i];
        }
        
        outDataArray = dataArray;
    } catch (...) {
        for (CCIUInt32 i = 0; i < arraySize; i++) {
            if (dataArray [i] != NULL) {
                delete static_cast <char*> (dataArray [i] -> data);
            }
            delete dataArray [i];
        }

        delete [] dataArray;
        
        throw;
    }
}