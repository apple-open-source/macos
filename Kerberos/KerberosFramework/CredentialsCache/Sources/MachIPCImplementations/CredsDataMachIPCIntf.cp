#include "CredsDataMachIPCStubs.h"
#include "CredentialsData.h"
#include "FlattenCredentials.h"

extern "C" {
    #include "CCacheIPCServer.h"
}

#include "MachIPCInterface.h"

kern_return_t CredentialsIPC_GetVersion (
	mach_port_t inServerPort,
	CredentialsID inCredentials,
	CCIUInt32 *outVersion,
	CCIResult *outResult) {

    try {
        CCICredentialsDataInterface	credentials (inCredentials);
        
        *outVersion = credentials -> GetVersion ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CredentialsIPC_Compare (
	mach_port_t inServerPort,
	CredentialsID	inCredentials,
	CredentialsID	inCompareTo,
	CCIUInt32*	outEqual,
	CCIResult*	outResult) {
        
    try {
        CCICredentialsDataInterface	credentials (inCredentials);
        
        *outEqual = credentials -> Compare (inCompareTo);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CredentialsIPC_FlattenCredentials (
	mach_port_t inServerPort,
	CredentialsID inCredentials,
	FlattenedOutCredentials *outCredentials,
	mach_msg_type_number_t *outCredentialsCnt,
	CCIResult *outResult) {

    try {
        CCICredentialsDataInterface	credentials (inCredentials);

        std::strstream		flatCredentials;
        flatCredentials << credentials.Get () << std::ends;
        CCIMachIPCServerBuffer <char>	buffer (flatCredentials.pcount ());

        memmove (buffer.Data (), flatCredentials.str (), buffer.Size ());
        flatCredentials.freeze (false);	// Makes sure the buffer will be deallocated

        *outCredentials = buffer.Data ();
        *outCredentialsCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

