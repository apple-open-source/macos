/*
 * CCIContextDataMachIPCStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/MachIPCImplementations/ContextDataMachIPCStubs.cp,v 1.16 2005/05/25 20:21:50 lxs Exp $
 */

#include "ContextDataMachIPCStubs.h"

extern "C" {
    #include "CCacheIPC.h"
}

CCIContextDataMachIPCStub::CCIContextDataMachIPCStub (
	CCIUniqueID			inContextID,
	CCIInt32			inAPIVersion):
	CCIContext (inContextID, inAPIVersion) {
}

CCIContextDataMachIPCStub::CCIContextDataMachIPCStub (
	CCIInt32			inAPIVersion):
	CCIContext (GetGlobalContextID (), inAPIVersion) {
}

CCIContextDataMachIPCStub::~CCIContextDataMachIPCStub () {
}

CCITime
CCIContextDataMachIPCStub::GetChangeTime () {
        CCIResult	result;
        mach_port_t port = GetPortNoLaunch ();
        
        if (port != MACH_PORT_NULL) {
            CCITime		changeTime;
            kern_return_t err = ContextIPC_GetChangeTime (port, GetContextID ().object, &changeTime, &result);
            if (err != KERN_SUCCESS) {
                // Special case this here because it isn't really an error.
                // Server died == ccache changed now.
                InvalidatePort ();
            } else {
                // Throw if result != ccNoError. 
                ThrowIfIPCError_ (err, result);
                
                // Server is running and we got a change time.  Remember it.
                UpdateStateChangedTimeFromServer (changeTime);
            }
        }
        
        return GetServerStateChangedTime ();
}


CCIUniqueID
CCIContextDataMachIPCStub::OpenCCache (
	const std::string&		inCCacheName) {
	
        CCacheID	ccache;
        CCIResult	result;
        mach_port_t port = GetPortNoLaunch ();
        
        if (port == MACH_PORT_NULL) {
            // No server running, so no ccaches to find
            ThrowIfIPCError_ (KERN_SUCCESS, ccErrCCacheNotFound);
        } else {
            kern_return_t err = ContextIPC_OpenCCache (port, GetContextID ().object, inCCacheName.c_str (), inCCacheName.length (), &ccache, &result);
            if (err != KERN_SUCCESS) {
                // Server died, no ccaches
                InvalidatePort ();
                ThrowIfIPCError_ (KERN_SUCCESS, ccErrCCacheNotFound);
            } else {
                ThrowIfIPCError_ (err, result);
            }
        }
        return ccache;
}

CCIUniqueID
CCIContextDataMachIPCStub::OpenDefaultCCache () {
        CCacheID	ccache;
        CCIResult	result;
        mach_port_t port = GetPortNoLaunch ();
        
        if (port == MACH_PORT_NULL) {
            // No server running, so no default ccache to find
            ThrowIfIPCError_ (KERN_SUCCESS, ccErrCCacheNotFound);
        } else {
            kern_return_t err = ContextIPC_OpenDefaultCCache (port, GetContextID ().object, &ccache, &result);
            if (err != KERN_SUCCESS) {
                // Server died, no default ccache
                InvalidatePort ();
                ThrowIfIPCError_ (KERN_SUCCESS, ccErrCCacheNotFound);
            } else {
                ThrowIfIPCError_ (err, result);
            }
        }
        return ccache;
}

std::string
CCIContextDataMachIPCStub::GetDefaultCCacheName () {
        CCIMachIPCBuffer <char>		buffer;
        CCIResult	result;
        mach_port_t port = GetPortNoLaunch ();
        
        if (port == MACH_PORT_NULL) {
            // No server running, so just return what the server's inital default cache will be
            return std::string (kInitialDefaultCCacheName);
        } else {
            kern_return_t err = ContextIPC_GetDefaultCCacheName (port, GetContextID ().object, &buffer.Data (), &buffer.Size (), &result);
            if (err != KERN_SUCCESS) {
                // Server died, return what default name would be
                InvalidatePort ();
                return std::string (kInitialDefaultCCacheName);
            } else {
                ThrowIfIPCError_ (err, result);
            }
        }
        return std::string (buffer.Data (), buffer.Count ());
}

CCIUniqueID
CCIContextDataMachIPCStub::CreateCCache (
	const std::string&		inName,
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
        CCacheID	ccache;
        CCIResult	result;
        kern_return_t err = ContextIPC_CreateCCache (GetPort (), GetContextID ().object, inName.c_str (), inName.length (), inVersion, inPrincipal.c_str (), inPrincipal.length (), &ccache, &result);
        ThrowIfIPCError_ (err, result);
        return ccache;
}

CCIUniqueID
CCIContextDataMachIPCStub::CreateDefaultCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
        CCacheID	ccache;
        CCIResult	result;
        kern_return_t err = ContextIPC_CreateDefaultCCache (GetPort (), GetContextID ().object, inVersion, inPrincipal.c_str (), inPrincipal.length (), &ccache, &result);
        ThrowIfIPCError_ (err, result);
        return ccache;
}

CCIUniqueID
CCIContextDataMachIPCStub::CreateNewCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
        CCacheID	ccache;
        CCIResult	result;
        kern_return_t err = ContextIPC_CreateNewCCache (GetPort (), GetContextID ().object, inVersion, inPrincipal.c_str (), inPrincipal.length (), &ccache, &result);
        ThrowIfIPCError_ (err, result);
        return ccache;
}

void CCIContextDataMachIPCStub::GetCCacheIDs (
		std::vector <CCIObjectID>&		outCCacheIDs) const {

        CCIMachIPCBuffer <CCacheID>	buffer;
        CCIResult	result;
        mach_port_t port = GetPortNoLaunch ();
        
        if (port == MACH_PORT_NULL) {
            // No server running, so return an empty list
            outCCacheIDs.resize (0);
        } else {
            kern_return_t err = ContextIPC_GetCCacheIDs (GetPort (), GetContextID ().object, &buffer.Data (), &buffer.Size (), &result);
            if (err != KERN_SUCCESS) {
                // Server died, return empty list
                InvalidatePort ();
                outCCacheIDs.resize (0);
            } else {
                ThrowIfIPCError_ (err, result);
                outCCacheIDs.resize (buffer.Count ());
                for (CCIUInt32 i = 0; i < buffer.Count (); i++) {
                    outCCacheIDs [i] = buffer.Data () [i];
                }
            }
        }
}

bool
CCIContextDataMachIPCStub::Compare (
    const CCIContext&	inCompareTo) const {
    
    CCIUInt32 equal;
    CCIResult	result;
    kern_return_t err = ContextIPC_Compare (GetPort (), GetContextID ().object, inCompareTo.GetContextID ().object, &equal, &result);
    ThrowIfIPCError_ (err, result);
    return (equal != 0);
}

CCIUniqueID
CCIContextDataMachIPCStub::GetGlobalContextID () const 
{
    ContextID			context;
    CCIResult			result;
    mach_port_t port = GetPortNoLaunch (); // Not GetPort () or we will loop
    
    if (port == MACH_PORT_NULL) {
        // No server running so return the default context ID.
        // When the server is launched later we will pick up the context id from it then
        context = 0;
    } else {
        kern_return_t err = ContextIPC_GetGlobalContextID (port, &context, &result);
        if (err != KERN_SUCCESS) {
            InvalidatePort ();
            context = 0;
        } else {
            ThrowIfIPCError_ (err, result);
        }
    }
    return context;
}

mach_port_t 
CCIContextDataMachIPCStub::GetPort () const
{
    pid_t lastServerPID = GetLastServerPID ();  // Note: get this first since GetPort may update it.
    mach_port_t port = CCIMachIPCStub::GetPort ();
    pid_t newServerPID = CCIMachIPCStub::GetServerPID (port);

    //dprintf ("%s(): port: %x / lastServerPID: %d / newServerPID: %d", __FUNCTION__, port, lastServerPID, newServerPID);
    if ((port != MACH_PORT_NULL) && (lastServerPID != newServerPID)) {
        // A new server: remember the context id:
        CCIUniqueID contextID = GetGlobalContextID ();
        dprintf ("%s(): got new context id %d", __FUNCTION__, contextID.object);
        SetContextID (contextID);
    }
    return port;
}

mach_port_t 
CCIContextDataMachIPCStub::GetPortNoLaunch () const
{
    pid_t lastServerPID = GetLastServerPID ();  // Note: get this first since GetPortNoLaunch may update it.
    mach_port_t port = CCIMachIPCStub::GetPortNoLaunch ();
    pid_t newServerPID = CCIMachIPCStub::GetServerPID (port);
    
    //dprintf ("%s(): port: %d / lastServerPID: %d / newServerPID: %d", __FUNCTION__, port, lastServerPID, newServerPID);
    if ((port != MACH_PORT_NULL) && (lastServerPID != newServerPID)) {
        // A new server: remember the context id:
        CCIUniqueID contextID = GetGlobalContextID ();
        dprintf ("%s(): got new context id %d", __FUNCTION__, contextID.object);
        SetContextID (contextID);
    }
    return port;
}


CCILockID
CCIContextDataMachIPCStub::Lock () {
    #warning CCIContextDataMachIPCStub::Lock() not implemented
    return 0;
}

void
CCIContextDataMachIPCStub::Unlock (
	CCILockID		inLock) {
    #warning CCIContextDataMachIPCStub::Unlock() not implemented
}
