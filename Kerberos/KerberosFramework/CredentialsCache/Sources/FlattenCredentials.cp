/*
 * CCIFlattenedCredentials.cp
 *
 * $Header$
 */

#include "FlattenCredentials.h"

// These functions write credentials to streams and extract them back from streams.
// Used to serialize credentials structures across various IPC layers.

void WriteCredentials (std::ostream& ioStream, const cc_credentials_union& inCredentials)
{
    //dprintf ("Entering %s(cc_credentials_union):", __FUNCTION__);
    
    // Output the version followed by the appropriate creds
    WriteUInt32 (ioStream, inCredentials.version);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
}

#define WriteStaticBuffer(stream,buffer) stream.write ((char *)&buffer, sizeof (buffer));
#define ReadStaticBuffer(stream,buffer)  stream.read ((char *)&buffer, sizeof (buffer));

void WriteV4Credentials (std::ostream& ioStream, const cc_credentials_v4_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    WriteUInt32       (ioStream, inCredentials.version);
    WriteStaticBuffer (ioStream, inCredentials.principal);
    WriteStaticBuffer (ioStream, inCredentials.principal_instance);
    WriteStaticBuffer (ioStream, inCredentials.service);
    WriteStaticBuffer (ioStream, inCredentials.service_instance);
    WriteStaticBuffer (ioStream, inCredentials.realm);
    WriteStaticBuffer (ioStream, inCredentials.session_key);
    WriteInt32        (ioStream, inCredentials.kvno);
    WriteInt32        (ioStream, inCredentials.string_to_key_type);
    WriteUInt32       (ioStream, inCredentials.issue_date);
    WriteInt32        (ioStream, inCredentials.lifetime);
    WriteStaticBuffer (ioStream, inCredentials.address);  // in_addr_t already in network byte order
    WriteInt32        (ioStream, inCredentials.ticket_size);
    WriteStaticBuffer (ioStream, inCredentials.ticket);
    //dprintf ("Exiting %s():", __FUNCTION__);
}

void ReadV4Credentials (std::istream& ioStream, cc_credentials_v4_t& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    ReadUInt32       (ioStream, inCredentials.version);
    ReadStaticBuffer (ioStream, inCredentials.principal);
    ReadStaticBuffer (ioStream, inCredentials.principal_instance);
    ReadStaticBuffer (ioStream, inCredentials.service);
    ReadStaticBuffer (ioStream, inCredentials.service_instance);
    ReadStaticBuffer (ioStream, inCredentials.realm);
    ReadStaticBuffer (ioStream, inCredentials.session_key);
    ReadInt32        (ioStream, inCredentials.kvno);
    ReadInt32        (ioStream, inCredentials.string_to_key_type);
    ReadUInt32       (ioStream, inCredentials.issue_date);
    ReadInt32        (ioStream, inCredentials.lifetime);
    ReadStaticBuffer (ioStream, inCredentials.address);  // in_addr_t already in network byte order
    ReadInt32        (ioStream, inCredentials.ticket_size);
    ReadStaticBuffer (ioStream, inCredentials.ticket);
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
}


void ReadV4CompatCredentials (std::istream& ioStream, cc_credentials_v4_compat& inCredentials)
{
    CCIInt32 issue_date;
    CCIUInt32 oops;
    
    //dprintf ("Entering %s():", __FUNCTION__);    
    ReadStaticBuffer (ioStream, inCredentials.kversion);
    ReadStaticBuffer (ioStream, inCredentials.principal);
    ReadStaticBuffer (ioStream, inCredentials.principal_instance);
    ReadStaticBuffer (ioStream, inCredentials.service);
    ReadStaticBuffer (ioStream, inCredentials.service_instance);
    ReadStaticBuffer (ioStream, inCredentials.realm);
    ReadStaticBuffer (ioStream, inCredentials.session_key);
    ReadInt32        (ioStream, inCredentials.kvno);
    ReadInt32        (ioStream, inCredentials.str_to_key);
    ReadInt32        (ioStream, issue_date); inCredentials.issue_date = issue_date;
    ReadInt32        (ioStream, inCredentials.lifetime);
    ReadStaticBuffer (ioStream, inCredentials.address);  // in_addr_t already in network byte order
    ReadInt32        (ioStream, inCredentials.ticket_sz);
    ReadStaticBuffer (ioStream, inCredentials.ticket);
    ReadUInt32       (ioStream, oops); inCredentials.oops = oops;
    //dprintf ("Exiting %s():", __FUNCTION__);
}

void WriteV4CompatCredentials (std::ostream& ioStream, const cc_credentials_v4_compat& inCredentials)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    WriteStaticBuffer (ioStream, inCredentials.kversion);
    WriteStaticBuffer (ioStream, inCredentials.principal);
    WriteStaticBuffer (ioStream, inCredentials.principal_instance);
    WriteStaticBuffer (ioStream, inCredentials.service);
    WriteStaticBuffer (ioStream, inCredentials.service_instance);
    WriteStaticBuffer (ioStream, inCredentials.realm);
    WriteStaticBuffer (ioStream, inCredentials.session_key);
    WriteInt32        (ioStream, inCredentials.kvno);
    WriteInt32        (ioStream, inCredentials.str_to_key);
    WriteInt32        (ioStream, inCredentials.issue_date);
    WriteInt32        (ioStream, inCredentials.lifetime);
    WriteStaticBuffer (ioStream, inCredentials.address);  // in_addr_t already in network byte order
    WriteInt32        (ioStream, inCredentials.ticket_sz);
    WriteStaticBuffer (ioStream, inCredentials.ticket);
    WriteUInt32       (ioStream, inCredentials.oops);
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    //dprintf ("Exiting %s():", __FUNCTION__);
}

#endif // CCache_v2_Compat

void WriteData (std::ostream& ioStream, const cc_data& inData)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    
    WriteUInt32 (ioStream, inData.type);
    WriteUInt32 (ioStream, inData.length);
    //dprintf ("%s(): Wrote type %d, length %d", __FUNCTION__, inData.type, inData.length);
    if (inData.length > 0) {
        ioStream.write ((char *)inData.data, inData.length);
        //dprintmem (inData.data, inData.length);
    }
    //dprintf ("Exiting %s():", __FUNCTION__);
}

void ReadData (std::istream& ioStream, cc_data& inData)
{
    //dprintf ("Entering %s():", __FUNCTION__);
    
    ReadUInt32 (ioStream, inData.type);
    ReadUInt32 (ioStream, inData.length);
    //dprintf ("%s(): Read type %d, length %d", __FUNCTION__, inData.type, inData.length);
    if (inData.length > 0) {
        inData.data = new char [inData.length];
        ioStream.read ((char *)inData.data, inData.length);
        //dprintmem (inData.data, inData.length);
    } else {
        inData.data = NULL;
    }
    //dprintf ("Exiting %s():", __FUNCTION__);
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
    
    ReadUInt32 (ioStream, length);
    outString = new char [length];
    ioStream.read ((char *)outString, length);
    //dprintf ("%s(): Reading string '%s'", __FUNCTION__, outString);
}

void WriteString (std::ostream& ioStream, const char *inString)
{
    CCIUInt32 length = strlen (inString) + 1;
    
    WriteUInt32 (ioStream, strlen (inString) + 1);
    ioStream.write (inString, length);
    //dprintf ("%s(): Wrote string '%s'", __FUNCTION__, inString);
}

void ReadString (std::istream& ioStream, std::string &outString)
{
    char *string = NULL;
    
    try {
        ReadString (ioStream, string);
        outString = string;
        delete [] string;
    } catch (...) {
        if (string != NULL) { delete [] string; }
        throw;
    }
    //dprintf ("%s(): Reading std::string '%s'", __FUNCTION__, outString.c_str());
}

void WriteString (std::ostream& ioStream, const std::string inString)
{
    WriteString (ioStream, inString.c_str ());
    //dprintf ("%s(): Writing std::string '%s'", __FUNCTION__, inString.c_str ());
}

void WriteInt32 (std::ostream& ioStream, CCIInt32 integer)
{
    //dprintf ("%s(): Writing integer '%d'", __FUNCTION__, integer);
    integer = htonl(integer);
    ioStream.write ((char *)&integer, sizeof (integer));
}

void ReadInt32 (std::istream& ioStream, CCIInt32& integer)
{
    ioStream.read ((char *)&integer, sizeof (integer));
    integer = ntohl(integer);
    //dprintf ("%s(): Reading integer '%d'", __FUNCTION__, integer);
}

void WriteUInt32 (std::ostream& ioStream, CCIUInt32 integer)
{
    //dprintf ("%s(): Writing integer '%d'", __FUNCTION__, integer);
    integer = htonl(integer);
    ioStream.write ((char *)&integer, sizeof (integer));
}

void ReadUInt32 (std::istream& ioStream, CCIUInt32& integer)
{
    ioStream.read ((char *)&integer, sizeof (integer));
    integer = ntohl(integer);
    //dprintf ("%s(): Reading integer '%d'", __FUNCTION__, integer);
}
