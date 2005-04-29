/*
 * CCIFlattenedCredentials.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/FlattenCredentials.cp,v 1.14 2004/09/08 20:48:35 lxs Exp $
 */

#include "FlattenCredentials.h"

// These functions write credentials to streams and extract them back from streams.
// Used to serialize credentials structures across various IPC layers.

void WriteCredentials (std::ostream& ioStream, const cc_credentials_union& inCredentials)
{
    CCIUInt32 version = inCredentials.version;
    //dprintf ("Entering %s(cc_credentials_union):", __FUNCTION__);
    
    // Output the version followed by the appropriate creds
    ioStream.write ((char *)&version, sizeof (version));
    switch (inCredentials.version) {
        case cc_credentials_v4:
            WriteV4Credentials (ioStream, *inCredentials.credentials.credentials_v4);
            break;
            
        case cc_credentials_v5:
            WriteV5Credentials (ioStream, *inCredentials.credentials.credentials_v5);
            break;
            
        default:
            throw CCIException (ccErrBadCredentialsVersion);
    }
}

void WriteV4Credentials (std::ostream& ioStream, const cc_credentials_v4_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.write ((char *)&inCredentials, sizeof (inCredentials));
}

void ReadV4Credentials (std::istream& ioStream, cc_credentials_v4_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.read ((char *)&inCredentials, sizeof (inCredentials));
}

void WriteV5Credentials (std::ostream& ioStream, const cc_credentials_v5_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    // Write out various parts of the creds in order
    WriteString    (ioStream, inCredentials.client);
    WriteString    (ioStream, inCredentials.server);
    WriteData      (ioStream, inCredentials.keyblock);
    WriteUInt32    (ioStream, inCredentials.authtime);
    WriteUInt32    (ioStream, inCredentials.starttime);
    WriteUInt32    (ioStream, inCredentials.endtime);
    WriteUInt32    (ioStream, inCredentials.renew_till);
    WriteUInt32    (ioStream, inCredentials.is_skey);
    WriteUInt32    (ioStream, inCredentials.ticket_flags);
    WriteDataArray (ioStream, inCredentials.addresses);
    WriteData      (ioStream, inCredentials.ticket);
    WriteData      (ioStream, inCredentials.second_ticket);
    WriteDataArray (ioStream, inCredentials.authdata);
}

void ReadV5Credentials (std::istream& ioStream, cc_credentials_v5_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    // Suck in the various parts of the creds in order
    try {
        ReadString    (ioStream, inCredentials.client);
        ReadString    (ioStream, inCredentials.server);
        ReadData      (ioStream, inCredentials.keyblock);
        ReadUInt32    (ioStream, inCredentials.authtime);
        ReadUInt32    (ioStream, inCredentials.starttime);
        ReadUInt32    (ioStream, inCredentials.endtime);
        ReadUInt32    (ioStream, inCredentials.renew_till);
        ReadUInt32    (ioStream, inCredentials.is_skey);
        ReadUInt32    (ioStream, inCredentials.ticket_flags);
        ReadDataArray (ioStream, inCredentials.addresses);
        ReadData      (ioStream, inCredentials.ticket);
        ReadData      (ioStream, inCredentials.second_ticket);
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
}

#if CCache_v2_compat

void WriteCompatCredentials (std::ostream& ioStream, const cred_union& inCredentials)
{
    //dprintf ("Entering %s(cred_union):", __FUNCTION__);
    WriteUInt32 (ioStream, inCredentials.cred_type);
    switch (inCredentials.cred_type) {
        case CC_CRED_V4:
            WriteV4CompatCredentials (ioStream, *inCredentials.cred.pV4Cred);
            break;
            
        case CC_CRED_V5:
            WriteV5CompatCredentials (ioStream, *inCredentials.cred.pV5Cred);
            break;
            
        default:
            throw CCIException (ccErrBadCredentialsVersion);
    }
}


void ReadV4CompatCredentials (std::istream& ioStream, cc_credentials_v4_compat& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.read ((char *)&inCredentials, sizeof (inCredentials));
}

void WriteV4CompatCredentials (std::ostream& ioStream, const cc_credentials_v4_compat& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.write ((char *)&inCredentials, sizeof (inCredentials));
}

void WriteV5CompatCredentials (std::ostream& ioStream, const cc_credentials_v5_compat& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    WriteString    (ioStream, inCredentials.client);
    WriteString    (ioStream, inCredentials.server);
    WriteData      (ioStream, inCredentials.keyblock);
    WriteUInt32    (ioStream, inCredentials.authtime);
    WriteUInt32    (ioStream, inCredentials.starttime);
    WriteUInt32    (ioStream, inCredentials.endtime);
    WriteUInt32    (ioStream, inCredentials.renew_till);
    WriteUInt32    (ioStream, inCredentials.is_skey);
    WriteUInt32    (ioStream, inCredentials.ticket_flags);
    WriteDataArray (ioStream, inCredentials.addresses);
    WriteData      (ioStream, inCredentials.ticket);
    WriteData      (ioStream, inCredentials.second_ticket);
    WriteDataArray (ioStream, inCredentials.authdata);
}

void ReadV5CompatCredentials (std::istream& ioStream, cc_credentials_v5_compat& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    try {
        ReadString    (ioStream, inCredentials.client);
        ReadString    (ioStream, inCredentials.server);
        ReadData      (ioStream, inCredentials.keyblock);
        ReadUInt32    (ioStream, inCredentials.authtime);
        ReadUInt32    (ioStream, inCredentials.starttime);
        ReadUInt32    (ioStream, inCredentials.endtime);
        ReadUInt32    (ioStream, inCredentials.renew_till);
        ReadUInt32    (ioStream, inCredentials.is_skey);
        ReadUInt32    (ioStream, inCredentials.ticket_flags);
        ReadDataArray (ioStream, inCredentials.addresses);
        ReadData      (ioStream, inCredentials.ticket);
        ReadData      (ioStream, inCredentials.second_ticket);
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
}

#endif // CCache_v2_Compat

void WriteData (std::ostream& ioStream, const cc_data& inData)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    
    WriteUInt32 (ioStream, inData.type);
    WriteUInt32 (ioStream, inData.length);
    //dprintf ("Wrote type %d, length %d", inData.type, inData.length);
    if (inData.length > 0) {
        ioStream.write ((char *)inData.data, inData.length);
        //dprintmem (inData.data, inData.length);
    }
}

void ReadData (std::istream& ioStream, cc_data& inData)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    
    ReadUInt32 (ioStream, inData.type);
    ReadUInt32 (ioStream, inData.length);
    //dprintf ("Read type %d, length %d", inData.type, inData.length);
    if (inData.length > 0) {
        inData.data = new char [inData.length];
        ioStream.read ((char *)inData.data, inData.length);
        //dprintmem (inData.data, inData.length);
    } else {
        inData.data = NULL;
    }
}

void WriteDataArray (std::ostream& ioStream,  cc_data** inDataArray)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    CCIUInt32 arraySize = 0;
    if (inDataArray != NULL) {
        while (inDataArray [arraySize] != NULL) { arraySize++; }
    }
    
    WriteUInt32 (ioStream, arraySize);
    for (CCIUInt32 i = 0; i < arraySize; i++) {
        WriteData (ioStream, *(inDataArray [i]));
    }
    //dprintf ("Exiting %s():", __FUNCTION__);
}

void ReadDataArray (std::istream& ioStream, cc_data**& outDataArray)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    CCIUInt32	arraySize = 0;
    ReadUInt32 (ioStream, arraySize);
    
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
            ReadData (ioStream, *dataArray [i]);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
}

void ReadString (std::istream& ioStream, char *&outString)
{
    CCIUInt32 length;
    
    //dprintf ("Entering %s():", __FUNCTION__);
    ReadUInt32 (ioStream, length);
    outString = new char [length];
    ioStream.read ((char *)outString, length);
    //dprintf ("Read string '%s'", outString);
}

void WriteString (std::ostream& ioStream, const char *inString)
{
    CCIUInt32 length = strlen (inString) + 1;
    
    //dprintf ("Entering %s():", __FUNCTION__);
    WriteUInt32 (ioStream, strlen (inString) + 1);
    ioStream.write (inString, length);
    //dprintf ("Wrote string '%s'", inString);
}

void ReadString (std::istream& ioStream, std::string &outString)
{
    char *string = NULL;
    
    //dprintf ("Entering %s():", __FUNCTION__);
    try {
        ReadString (ioStream, string);
        //dprintf ("Intermediate string '%s'", string);
        outString = string;
        delete [] string;
    } catch (...) {
        if (string != NULL) { delete [] string; }
        throw;
    }
    //dprintf ("Read std::string '%s'", outString.c_str());
}

void WriteString (std::ostream& ioStream, const std::string inString)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    WriteString (ioStream, inString.c_str ());
    //dprintf ("Wrote std::string '%s'", inString.c_str ());
}

void WriteUInt32 (std::ostream& ioStream, CCIUInt32 integer)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.write ((char *)&integer, sizeof (integer));
    //dprintf ("Wrote integer '%d'", integer);
}

void ReadUInt32 (std::istream& ioStream, CCIUInt32& integer)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ioStream.read ((char *)&integer, sizeof (integer));
    //dprintf ("Read integer '%d'", integer);
}
