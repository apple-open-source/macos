/*
 * CCICCacheDataMachIPCStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/MachIPCImplementations/CCacheDataMachIPCStubs.cp,v 1.15 2005/05/25 20:23:00 lxs Exp $
 */

#include "FlattenCredentials.h"
#include "CCacheDataMachIPCStubs.h"

extern "C" {
    #include "CCacheIPC.h"
};

CCICCacheDataMachIPCStub::CCICCacheDataMachIPCStub (
	CCIUniqueID	inCCache,
	CCIInt32	inAPIVersion):
	CCICCache (inCCache, inAPIVersion) {
}

CCICCacheDataMachIPCStub::~CCICCacheDataMachIPCStub () {
}

void
CCICCacheDataMachIPCStub::Destroy () {
        CCIResult	result;
        kern_return_t err = CCacheIPC_Destroy (GetPort (), GetCCacheID ().object, &result);
        ThrowIfIPCError_ (err, result);
}

void
CCICCacheDataMachIPCStub::SetDefault () {
        CCIResult	result;
        kern_return_t err = CCacheIPC_SetDefault (GetPort (), GetCCacheID ().object, &result);
        ThrowIfIPCError_ (err, result);
}

CCIUInt32
CCICCacheDataMachIPCStub::GetCredentialsVersion () {
        CCIUInt32	version;
        CCIResult	result;
        kern_return_t err = CCacheIPC_GetCredentialsVersion (GetPort (), GetCCacheID ().object, &version, &result);
        ThrowIfIPCError_ (err, result);
	return version;
}

std::string
CCICCacheDataMachIPCStub::GetPrincipal (
	CCIUInt32				inVersion) {
	
        CCIResult	result;
        CCIMachIPCBuffer <char>		buffer;
        kern_return_t err = CCacheIPC_GetPrincipal (GetPort (), GetCCacheID ().object, inVersion, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
	return std::string (buffer.Data (), buffer.Count ());
}
	
std::string
CCICCacheDataMachIPCStub::GetName () {
	
        CCIResult	result;
        CCIMachIPCBuffer <char>		buffer;
        kern_return_t err = CCacheIPC_GetName (GetPort (), GetCCacheID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
	return std::string (buffer.Data (), buffer.Count ());
}
	
void
CCICCacheDataMachIPCStub::SetPrincipal (
	CCIUInt32			inVersion,
	const std::string&		inPrincipal) {
	
        CCIResult	result;
        kern_return_t err = CCacheIPC_SetPrincipal (GetPort (), GetCCacheID ().object, inVersion, inPrincipal.c_str (), inPrincipal.length (), &result);
        ThrowIfIPCError_ (err, result);
}
	
#if CCache_v2_compat
void
CCICCacheDataMachIPCStub::CompatSetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
        CCIResult	result;
        kern_return_t err = CCacheIPC_CompatSetPrincipal (GetPort (), GetCCacheID ().object, inVersion, inPrincipal.c_str (), inPrincipal.length (), &result);
        ThrowIfIPCError_ (err, result);
}
#endif	

void
CCICCacheDataMachIPCStub::StoreConvertedCredentials (
	const cc_credentials_union*		inCredentials) {

        CCIResult			result;
        std::strstream		flatCredentials;
        
        WriteCredentials (flatCredentials, *inCredentials);
        //dprintf ("%s(): sending buffer:", __FUNCTION__);
        //dprintmem (flatCredentials.str (), flatCredentials.pcount ());
        kern_return_t err = CCacheIPC_StoreCredentials (GetPort (), GetCCacheID ().object, flatCredentials.str (), flatCredentials.pcount (), &result);
        flatCredentials.freeze (false);	// Makes sure the buffer will be deallocated
        ThrowIfIPCError_ (err, result);
}
	
void
CCICCacheDataMachIPCStub::StoreFlattenedCredentials (
	std::strstream&		inCredentials) {

        CCIResult			result;
        
        //dprintf ("%s(): sending buffer:", __FUNCTION__);
        //dprintmem (inCredentials.str (), inCredentials.pcount ());
        
        kern_return_t err = CCacheIPC_StoreCredentials (GetPort (), GetCCacheID ().object, inCredentials.str (), inCredentials.pcount (), &result);
        ThrowIfIPCError_ (err, result);
}
	
#if CCache_v2_compat
void
CCICCacheDataMachIPCStub::CompatStoreConvertedCredentials (
	const cred_union&		inCredentials) {

        CCIResult			result;
        std::strstream		flatCredentials;

        WriteCompatCredentials (flatCredentials, inCredentials);
        //dprintf ("%s(): sending buffer:", __FUNCTION__);
        //dprintmem (flatCredentials.str (), flatCredentials.pcount ());

        kern_return_t err = CCacheIPC_StoreCredentials (GetPort (), GetCCacheID ().object, flatCredentials.str (), flatCredentials.pcount (), &result);
        flatCredentials.freeze (false);	// Makes sure the buffer will be deallocated
        ThrowIfIPCError_ (err, result);
}

#endif*/
	
void		
CCICCacheDataMachIPCStub::RemoveCredentials (
	const CCICredentials&	inCredentials) {

        CCIResult	result;
        kern_return_t err = CCacheIPC_RemoveCredentials (GetPort (), GetCCacheID ().object, inCredentials.GetCredentialsID ().object, &result);
        ThrowIfIPCError_ (err, result);
}
	
CCITime		
CCICCacheDataMachIPCStub::GetLastDefaultTime () {

        CCITime	time;
        CCIResult	result;
        kern_return_t err = CCacheIPC_GetLastDefaultTime (GetPort (), GetCCacheID ().object, &time, &result);
        ThrowIfIPCError_ (err, result);
	return time;
}

CCITime		
CCICCacheDataMachIPCStub::GetChangeTime () {
	
        CCITime	time;
        CCIResult	result;
        kern_return_t err = CCacheIPC_GetChangeTime (GetPort (), GetCCacheID ().object, &time, &result);
        ThrowIfIPCError_ (err, result);
	return time;
}

void		
CCICCacheDataMachIPCStub::Move (
	CCICCache&		inCCache) {
	
        CCIResult	result;
        kern_return_t err = CCacheIPC_Move (GetPort (), GetCCacheID ().object, inCCache.GetCCacheID ().object, &result);
        ThrowIfIPCError_ (err, result);
}

CCILockID		
CCICCacheDataMachIPCStub::Lock () {
        #warning CCICCacheDataMachIPCStub::Lock() not implemented
        return 0;
}

void		
CCICCacheDataMachIPCStub::Unlock (
	CCILockID					inLock) {
	
        #warning CCICCacheDataMachIPCStub::Unlock() not implemented
}

bool
CCICCacheDataMachIPCStub::Compare (
    const CCICCache&	inCompareTo) const {
    
    CCIUInt32 equal;
    CCIResult	result;
    kern_return_t err = CCacheIPC_Compare (GetPort (), GetCCacheID ().object, inCompareTo.GetCCacheID ().object, &equal, &result);
    ThrowIfIPCError_ (err, result);
    return (equal != 0);
}

void		
CCICCacheDataMachIPCStub::GetCredentialsIDs (
	std::vector <CCIObjectID>&	outCredenitalsIDs) const {

        CCIMachIPCBuffer <CredentialsID>	buffer;
        CCIResult	result;
        kern_return_t err = CCacheIPC_GetCredentialsIDs (GetPort (), GetCCacheID ().object, &buffer.Data (), &buffer.Size (), &result);
        ThrowIfIPCError_ (err, result);
        outCredenitalsIDs.resize (buffer.Count ());
        for (CCIUInt32 i = 0; i < buffer.Count (); i++) {
            outCredenitalsIDs [i] = buffer.Data () [i];
        }
}


CCITime
CCICCacheDataMachIPCStub::GetKDCTimeOffset (
    	CCIUInt32				inVersion) const 
{
    CCITime	timeOffset;
    CCIResult	result;
    kern_return_t err = CCacheIPC_GetKDCTimeOffset (GetPort (), GetCCacheID ().object, inVersion, &timeOffset, &result);
    ThrowIfIPCError_ (err, result);
	return timeOffset;

}


void
CCICCacheDataMachIPCStub::SetKDCTimeOffset (
    	CCIUInt32				inVersion,
        CCITime					inTimeOffset)
{
    CCIResult	result;
    kern_return_t err = CCacheIPC_SetKDCTimeOffset (GetPort (), GetCCacheID ().object, inVersion, inTimeOffset, &result);
    ThrowIfIPCError_ (err, result);

}

void
CCICCacheDataMachIPCStub::ClearKDCTimeOffset (
    	CCIUInt32				inVersion)
{
    CCIResult	result;
    kern_return_t err = CCacheIPC_ClearKDCTimeOffset (GetPort (), GetCCacheID ().object, inVersion, &result);
    ThrowIfIPCError_ (err, result);

}
