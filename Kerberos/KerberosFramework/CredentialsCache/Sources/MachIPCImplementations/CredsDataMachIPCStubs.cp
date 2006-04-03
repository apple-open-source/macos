/*
 * CCICredentialsDataMachIPCStubs.cp
 *
 * $Header$
 */
 
#include "CredsDataMachIPCStubs.h"
#include "FlattenCredentials.h"

extern "C" {
    #include "CCacheIPC.h"
}

CCICredentialsDataMachIPCStub::CCICredentialsDataMachIPCStub (
	CCIUniqueID	inCredentials,
	CCIInt32	inAPIVersion,
	bool		inInitialize):
	CCICredentials (inCredentials, inAPIVersion) {
	
	if (inInitialize)
		Initialize ();
}

CCICredentialsDataMachIPCStub::~CCICredentialsDataMachIPCStub () {
}

bool
CCICredentialsDataMachIPCStub::Compare (
    const CCICredentials&	inCompareTo) const {
    
    CCIUInt32 equal;
    CCIResult	result;
    kern_return_t err = CredentialsIPC_Compare (GetPort (), GetCredentialsID ().object, inCompareTo.GetCredentialsID ().object, &equal, &result);
    ThrowIfIPCError_ (err, result);
    return (equal != 0);
}

CCIUInt32
CCICredentialsDataMachIPCStub::GetCredentialsVersion () {

        CCIResult	result;
        CCIUInt32	version;
        kern_return_t err = CredentialsIPC_GetVersion (GetPort (), GetCredentialsID ().object, &version, &result);
        ThrowIfIPCError_ (err, result);
	return version;
}

void
CCICredentialsDataMachIPCStub::FlattenToStream (
	std::ostream&		outFlatCredentials) const {

        CCIResult			result;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        outFlatCredentials << buffer.Data () << std::ends;
}

void
CCICredentialsDataMachIPCStub::CopyV4Credentials (
	cc_credentials_v4_t&		outCredentials) const {

        CCIResult			result;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v4);
        ReadV4Credentials (flatCredentials, outCredentials);
}

void
CCICredentialsDataMachIPCStub::CopyV5Credentials (
	cc_credentials_v5_t&		outCredentials) const {

        CCIResult			result;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32 version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v5);
        ReadV5Credentials (flatCredentials, outCredentials);
}

#if CCache_v2_compat
void
CCICredentialsDataMachIPCStub::CompatCopyV4Credentials (
	cc_credentials_v4_compat&		outCredentials) const {

        CCIResult			result;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v4);
        ReadV4CompatCredentials (flatCredentials, outCredentials);
}

void
CCICredentialsDataMachIPCStub::CompatCopyV5Credentials (
	cc_credentials_v5_compat&		outCredentials) const {

        CCIResult			result;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v5);
        ReadV5CompatCredentials (flatCredentials, outCredentials);
}
#endif
