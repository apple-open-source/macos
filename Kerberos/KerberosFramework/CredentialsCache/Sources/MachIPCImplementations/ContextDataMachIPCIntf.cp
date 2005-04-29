#include "ContextDataMachIPCStubs.h"
#include "ContextData.h"

extern "C" {
    #include "CCacheIPCServer.h"
}

#include "MachIPCInterface.h"
#ifdef Classic_Ticket_Sharing
#  include "HandleBuffer.h"
#  include "ClassicProtocol.h"
#  include "ClassicSupport.h"
#endif

#include <Kerberos/mach_server_utilities.h>

kern_return_t ContextIPC_GetChangeTime (
	mach_port_t inServerPort,
	ContextID inContext,
	CCITime *outTime,
	CCIResult *outResult) {
        
    try {
        CCIContextDataInterface	context (inContext);
        *outTime = context -> GetChangeTime ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}
        

kern_return_t ContextIPC_OpenCCache (
	mach_port_t inServerPort,
	ContextID inContext,
	CCacheInName inName,
	mach_msg_type_number_t inNameCnt,
	CCacheID *outCCacheID,
	CCIResult *outResult) {

    try {
        CCIContextDataInterface	context (inContext);
        *outCCacheID = context -> GetCCacheID (std::string (inName, inNameCnt)).object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_OpenDefaultCCache (
	mach_port_t inServerPort,
	ContextID inContext,
	CCacheID *outCCacheID,
	CCIResult *outResult) {

    try {
        CCIContextDataInterface	context (inContext);
        *outCCacheID = context -> GetDefaultCCacheID ().object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_GetDefaultCCacheName (
	mach_port_t inServerPort,
	ContextID inContext,
	CCacheOutName *outCCacheName,
	mach_msg_type_number_t *outCCacheNameCnt,
	CCIResult *outResult) {
        
    try {
        CCIContextDataInterface	context (inContext);
        
        std::string	defaultName = context -> GetDefaultCCacheName ();
        
        CCIMachIPCServerBuffer <char>		buffer (defaultName.length ());

        memmove (buffer.Data (), defaultName.c_str (), buffer.Size ());

        *outCCacheName = buffer.Data ();
        *outCCacheNameCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}        

kern_return_t ContextIPC_CreateCCache (
	mach_port_t inServerPort,
	ContextID inContext,
	CCacheInName inCCacheName,
	mach_msg_type_number_t inCCacheNameCnt,
	CCIUInt32 inCCacheVersion,
	CCacheInPrincipal inCCachePrincipal,
	mach_msg_type_number_t inCCachePrincipalCnt,
	CCacheID *outCCacheID,
	CCIResult *outResult) {
	
    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_Context_CreateCCache;
			buffer.Put (diffType);

			buffer.Put (inContext);
			buffer.Put (inCCacheNameCnt);
			buffer.PutData (inCCacheName, inCCacheNameCnt);
			buffer.Put (inCCacheVersion);
			buffer.Put (inCCachePrincipalCnt);
			buffer.PutData (inCCachePrincipal, inCCachePrincipalCnt);
			
			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif
                
        CCIContextDataInterface	context (inContext);
        *outCCacheID = context -> CreateCCache (std::string (inCCacheName, inCCacheNameCnt), inCCacheVersion, std::string (inCCachePrincipal, inCCachePrincipalCnt)).object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
	
	if (*outResult != ccNoError) {
#ifdef Classic_Ticket_Sharing
		CCIClassicSupport::RemoveLastDiff ();
#endif        
        // Check to see if the ccache is empty (we launched to add this and failed).  
        // If it is, we should quit.
        if (CCIUniqueGlobally <CCICCacheData>::CountGloballyUniqueIDs () == 0) {
            mach_server_quit_self ();
        }
	}
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_CreateDefaultCCache (
	mach_port_t inServerPort,
	ContextID inContext,
	CCIUInt32 inCCacheVersion,
	CCacheInPrincipal inCCachePrincipal,
	mach_msg_type_number_t inCCachePrincipalCnt,
	CCacheID *outCCacheID,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_Context_CreateDefaultCCache;
			buffer.Put (diffType);

			buffer.Put (inContext);
			buffer.Put (inCCacheVersion);
			buffer.Put (inCCachePrincipalCnt);
			buffer.PutData (inCCachePrincipal, inCCachePrincipalCnt);
			
			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif
        CCIContextDataInterface	context (inContext);
        *outCCacheID = context -> CreateDefaultCCache (inCCacheVersion, std::string (inCCachePrincipal, inCCachePrincipalCnt)).object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
	if (*outResult != ccNoError) {
#ifdef Classic_Ticket_Sharing
		CCIClassicSupport::RemoveLastDiff ();
#endif        
        // Check to see if the ccache is empty (we launched to add this and failed).  
        // If it is, we should quit.
        if (CCIUniqueGlobally <CCICCacheData>::CountGloballyUniqueIDs () == 0) {
            mach_server_quit_self ();
        }
	}
    
    return KERN_SUCCESS;
}        

kern_return_t ContextIPC_CreateNewCCache (
	mach_port_t inServerPort,
	ContextID inContext,
	CCIUInt32 inCCacheVersion,
	CCacheInPrincipal inCCachePrincipal,
	mach_msg_type_number_t inCCachePrincipalCnt,
	CCacheID *outCCacheID,
	CCIResult *outResult) {

    try {
#ifdef Classic_Ticket_Sharing
		CCIHandleBuffer		buffer;
		
		if (CCIClassicSupport::KeepDiffs ()) {
			CCIUInt32			diffType = ccClassic_Context_CreateNewCCache;
			buffer.Put (diffType);

			buffer.Put (inContext);
			buffer.Put (inCCacheVersion);
			buffer.Put (inCCachePrincipalCnt);
			buffer.PutData (inCCachePrincipal, inCCachePrincipalCnt);
			
			CCIClassicSupport::SaveOneDiff (buffer.GetHandle ());
			buffer.ReleaseHandle ();
		}
#endif
                
        CCIContextDataInterface	context (inContext);

        *outCCacheID = context -> CreateNewCCache (inCCacheVersion, std::string (inCCachePrincipal, inCCachePrincipalCnt)).object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
	if (*outResult != ccNoError) {
#ifdef Classic_Ticket_Sharing
		CCIClassicSupport::RemoveLastDiff ();
#endif
            
        // Check to see if the ccache is empty (we launched to add this and failed).  
        // If it is, we should quit.
        if (CCIUniqueGlobally <CCICCacheData>::CountGloballyUniqueIDs () == 0) {
            mach_server_quit_self ();
        }
	}
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_Compare (
	mach_port_t inServerPort,
	ContextID	inContext,
	ContextID	inCompareTo,
	CCIUInt32*	outEqual,
	CCIResult*	outResult) {
        
    try {
        CCIContextDataInterface	context (inContext);
        
        *outEqual = context -> Compare (inCompareTo);
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_GetCCacheIDs (
	mach_port_t inServerPort,
	ContextID inContext,
	CCacheIDArray *outCCacheIDs,
	mach_msg_type_number_t *outCCacheIDsCnt,
	CCIResult *outResult) {
        
    try {
        CCIContextDataInterface	context (inContext);
        
        std::vector <CCacheID>	ccacheIDs;
        context -> GetCCacheIDs (ccacheIDs);
        
        CCIMachIPCServerBuffer <CCacheID>	buffer (ccacheIDs.size ());

        for (CCIUInt32 i = 0; i < ccacheIDs.size (); i++) {
            buffer.Data () [i] = ccacheIDs [i];
        }

        *outCCacheIDs = buffer.Data ();
        *outCCacheIDsCnt = buffer.Size ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}

kern_return_t ContextIPC_GetGlobalContextID (
	mach_port_t inServerPort,
	ContextID* outContext,
	CCIResult *outResult) {
        
    try {
        *outContext = CCIContextDataInterface::GetGlobalContext () -> GetGloballyUniqueID ().object;
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}