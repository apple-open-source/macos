/*
 * CCICredentialsDataMachIPCStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/MachIPCImplementations/CredsDataMachIPCStubs.cp,v 1.12 2004/09/08 20:48:33 lxs Exp $
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
        security_token_t	token;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result, &token);
        if (!mach_client_allow_server (token)) {
            /* Warning!  This server is not who we think it is! */
            result = ccErrServerInsecure;
        }
        ThrowIfIPCError_ (err, result);
        outFlatCredentials << buffer.Data () << std::ends;
}

void
CCICredentialsDataMachIPCStub::CopyV4Credentials (
	cc_credentials_v4_t&		outCredentials) const {

        CCIResult			result;
        security_token_t	token;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result, &token);
        if (!mach_client_allow_server (token)) {
            /* Warning!  This server is not who we think it is! */
            result = ccErrServerInsecure;
        }
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        //flatCredentials >> version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v4);
        //flatCredentials >> outCredentials;
        ReadV4Credentials (flatCredentials, outCredentials);
}

void
CCICredentialsDataMachIPCStub::CopyV5Credentials (
	cc_credentials_v5_t&		outCredentials) const {

        CCIResult			result;
        security_token_t	token;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result, &token);
        if (!mach_client_allow_server (token)) {
            /* Warning!  This server is not who we think it is! */
            result = ccErrServerInsecure;
        }
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32 version;
        //flatCredentials >> version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v5);
        //flatCredentials >> outCredentials;
        ReadV5Credentials (flatCredentials, outCredentials);
}

#if CCache_v2_compat
void
CCICredentialsDataMachIPCStub::CompatCopyV4Credentials (
	cc_credentials_v4_compat&		outCredentials) const {

        CCIResult			result;
        security_token_t	token;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result, &token);
        if (!mach_client_allow_server (token)) {
            /* Warning!  This server is not who we think it is! */
            result = ccErrServerInsecure;
        }
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        //flatCredentials >> version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v4);
        //flatCredentials >> outCredentials;
        ReadV4CompatCredentials (flatCredentials, outCredentials);
}

void
CCICredentialsDataMachIPCStub::CompatCopyV5Credentials (
	cc_credentials_v5_compat&		outCredentials) const {

        CCIResult			result;
        security_token_t	token;
        CCIMachIPCBuffer <char>	buffer;
        
        kern_return_t err = CredentialsIPC_FlattenCredentials (GetPort (), GetCredentialsID ().object, &buffer.Data (), &buffer.Size (), &result, &token);
        if (!mach_client_allow_server (token)) {
            /* Warning!  This server is not who we think it is! */
            result = ccErrServerInsecure;
        }
        ThrowIfIPCError_ (err, result);
        std::istrstream		flatCredentials (buffer.Data (), buffer.Count ());

        CCIUInt32	version;
        //flatCredentials >> version;
        ReadUInt32 (flatCredentials, version);
        CCIAssert_ (version == cc_credentials_v5);
        //flatCredentials >> outCredentials;
        ReadV5CompatCredentials (flatCredentials, outCredentials);
}
#endif
