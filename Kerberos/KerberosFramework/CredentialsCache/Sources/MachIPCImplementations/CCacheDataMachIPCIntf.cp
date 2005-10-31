#include "FlattenCredentials.h"
#include "CCacheDataMachIPCStubs.h"
#include "CCacheData.h"
#include "CredentialsData.h"
#ifdef Classic_Ticket_Sharing
#  include "HandleBuffer.h"
#  include "ClassicProtocol.h"
#  include "ClassicSupport.h"
#endif

#include <Kerberos/mach_server_utilities.h>

extern "C" {
    #include "CCacheIPCServer.h"
};

#include "MachIPCInterface.h"

kern_return_t CCacheIPC_Destroy (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIResult *outResult) {
        
    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_Destroy;
			buffer.Put (diffType);
			
			buffer.Put (inCCache);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif

        CCICCacheDataInterface	ccache (inCCache);
        ccache -> Destroy ();

        // Check to see if the ccache is now empty.  If it is, we should quit.
        if (CCIUniqueGlobally <CCICCacheData>::CountGloballyUniqueIDs () == 0) {
            dprintf ("CCacheIPC_Destroy: destroying last ccache.... quitting.\n");
            mach_server_quit_self ();
        }
        
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}    

kern_return_t CCacheIPC_SetDefault (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_SetDefault;
			buffer.Put (diffType);

			buffer.Put (inCCache);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif

        CCICCacheDataInterface	ccache (inCCache);
        ccache -> SetDefault ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetCredentialsVersion (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIUInt32 *outCredentialsVersion,
	CCIResult *outResult) {

    try {
        CCICCacheDataInterface	ccache (inCCache);
        *outCredentialsVersion = ccache -> GetCredentialsVersion ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetPrincipal (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIUInt32 inVersion,
	CCacheOutPrincipal *outCCachePrincipal,
	mach_msg_type_number_t *outCCachePrincipalCnt,
	CCIResult *outResult) {

    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        std::string principal = ccache -> GetPrincipal (inVersion);
        
        CCIMachIPCServerBuffer <char>	buffer (principal.length ());

        memmove (buffer.Data (), principal.c_str (), buffer.Size ());

        *outCCachePrincipal = buffer.Data ();
        *outCCachePrincipalCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetName (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCacheOutName *outCCacheName,
	mach_msg_type_number_t *outCCacheNameCnt,
	CCIResult *outResult) {

    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        std::string name = ccache -> GetName ();
        CCIMachIPCServerBuffer <char>	buffer (name.length ());

        memmove (buffer.Data (), name.c_str (), buffer.Size ());

        *outCCacheName = buffer.Data ();
        *outCCacheNameCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_SetPrincipal (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIUInt32 inVersion,
	CCacheInPrincipal inCCachePrincipal,
	mach_msg_type_number_t inCCachePrincipalCnt,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_SetPrincipal;
			buffer.Put (diffType);

			buffer.Put (inCCache);
			buffer.Put (inVersion);
			buffer.Put (inCCachePrincipalCnt);
			buffer.PutData (inCCachePrincipal, inCCachePrincipalCnt);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif

        CCICCacheDataInterface	ccache (inCCache);
        
        ccache -> SetPrincipal (inVersion, std::string (inCCachePrincipal, inCCachePrincipalCnt));
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_CompatSetPrincipal (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCIUInt32 inVersion,
	CCacheInPrincipal inCCachePrincipal,
	mach_msg_type_number_t inCCachePrincipalCnt,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_CompatSetPrincipal;
			buffer.Put (diffType);

			buffer.Put (inCCache);
			buffer.Put (inVersion);
			buffer.Put (inCCachePrincipalCnt);
			buffer.PutData (inCCachePrincipal, inCCachePrincipalCnt);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif
        
        CCICCacheDataInterface	ccache (inCCache);
        
        ccache -> CompatSetPrincipal (inVersion, std::string (inCCachePrincipal, inCCachePrincipalCnt));
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_StoreCredentials (
	mach_port_t inServerPort,
	CCacheID inCCache,
	FlattenedInCredentials inCredentials,
	mach_msg_type_number_t inCredentialsCnt,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
        CCIHandleBuffer		buffer;
        
        if (CCIClassicSupport::KeepDiffs ()) {
            CCIUInt32			diffType = ccClassic_CCache_StoreConvertedCredentials;
            buffer.Put (diffType);
            
            buffer.Put (inCCache);
            buffer.Put (inCredentialsCnt);
            buffer.PutData (inCredentials, inCredentialsCnt);
            
            CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
            buffer.ReleaseHandle ();
        }
#endif
        
        //dprintf ("%s(): got flattened credentials buffer:", __FUNCTION__);
        //dprintmem (inCredentials, inCredentialsCnt);
        
        CCICCacheDataInterface	ccache (inCCache);
        
        std::istrstream		stream (inCredentials, inCredentialsCnt);
        CCICredentialsData*	newCreds = new CCICredentialsData (stream);
        ccache -> StoreCredentials (newCreds);
        
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_RemoveCredentials (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CredentialsID inCredentials,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_RemoveCredentials;
			buffer.Put (diffType);

			buffer.Put (inCCache);
			buffer.Put (inCredentials);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif

        CCICCacheDataInterface	ccache (inCCache);
        
        ccache -> RemoveCredentials (inCredentials);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetLastDefaultTime (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCITime *outLastDefaultTime,
	CCIResult *outResult) {

    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        *outLastDefaultTime = ccache -> GetLastDefaultTime ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetChangeTime (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CCITime *outChangeTime,
	CCIResult *outResult) {

    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        *outChangeTime = ccache -> GetChangeTime ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_Move (
	mach_port_t inServerPort,
	CCacheID inSourceCCache,
	CCacheID inDestinationCCache,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_CCache_Move;
			buffer.Put (diffType);

			buffer.Put (inSourceCCache);
			buffer.Put (inDestinationCCache);

			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif
                
        CCICCacheDataInterface	ccache (inSourceCCache);
        
        ccache -> Move (inDestinationCCache);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
#ifdef Classic_Ticket_Sharing
	if (*outResult != ccNoError) {
		CCIClassicSupport::RemoveLastDiff ();
	}
#endif
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_Compare (
	mach_port_t inServerPort,
	CCacheID	inCCache,
	CCacheID	inCompareTo,
	CCIUInt32*	outEqual,
	CCIResult*	outResult) {
        
    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        *outEqual = ccache -> Compare (inCompareTo);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetCredentialsIDs (
	mach_port_t inServerPort,
	CCacheID inCCache,
	CredentialsIDArray *outCredentialsIDs,
	mach_msg_type_number_t *outCredentialsIDsCnt,
	CCIResult *outResult) { 
               
    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        std::vector <CredentialsID>	credentialsIDs;
        ccache -> GetCredentialsIDs (credentialsIDs);
        
        CCIMachIPCServerBuffer <CredentialsID>	buffer (credentialsIDs.size ());

        for (CCIUInt32 i = 0; i < credentialsIDs.size (); i++) {
            (buffer.Data ()) [i] = credentialsIDs [i];
        }

        *outCredentialsIDs = buffer.Data ();
        *outCredentialsIDsCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_GetKDCTimeOffset (
    mach_port_t inServerPort,
    CCacheID	inCCache,
    CCIUInt32	inVersion,
    CCITime 	*outTimeOffset,
	CCIResult 	*outResult)
{
    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        *outTimeOffset = ccache -> GetKDCTimeOffset (inVersion);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_SetKDCTimeOffset (
    mach_port_t inServerPort,
    CCacheID	inCCache,
    CCIUInt32	inVersion,
    CCITime 	inTimeOffset,
	CCIResult 	*outResult)
{
    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        ccache -> SetKDCTimeOffset (inVersion, inTimeOffset);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t CCacheIPC_ClearKDCTimeOffset (
    mach_port_t inServerPort,
    CCacheID	inCCache,
    CCIUInt32	inVersion,
	CCIResult 	*outResult)
{
    try {
        CCICCacheDataInterface	ccache (inCCache);
        
        ccache -> ClearKDCTimeOffset (inVersion);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}
