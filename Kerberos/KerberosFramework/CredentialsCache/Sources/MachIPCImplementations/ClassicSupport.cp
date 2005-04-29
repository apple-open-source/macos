#include <Carbon/Carbon.h>

#include "MachIPCInterface.h"
#include "ContextData.h"
#include "CCacheData.h"
#ifdef Classic_Ticket_Sharing
#  include "ClassicSupport.h"
#  include "ClassicProtocol.h"
#  include "HandleBuffer.h"
#endif

std::list <Handle>			CCIClassicSupport::sDiffs;
CCIUInt32				CCIClassicSupport::sFirstSeqNo = 0;
CCIUInt32				CCIClassicSupport::sNextSeqNo = 0;
bool					CCIClassicSupport::sKeepDiffs = false;

bool CCIClassicSupport::KeepDiffs ()
{
    // If we have too many diffs, assume Classic died.
    // We would have a better way to do this if Processes.h weren't in Carbon.framework
    if (sKeepDiffs && (sNextSeqNo - sFirstSeqNo >= 10000)) {
        dprintf ("Turning off diffs because Classic isn't running anymore.\n");
        sKeepDiffs = false;
        RemoveAllDiffs ();
    }
    return sKeepDiffs;
}
		
void CCIClassicSupport::SaveOneDiff (
	Handle		inDiff)
{
    if (KeepDiffs ()) {
        dprintf ("Saving CCache diff with seqno %d.\n", sNextSeqNo);
	sDiffs.push_back (inDiff);
	sNextSeqNo++;
    }
}

void CCIClassicSupport::RemoveLastDiff ()
{
        if (KeepDiffs ()) {
            if (sDiffs.begin () != sDiffs.end ()) {
                dprintf ("Removing one CCache diff at seqno %d\n", sNextSeqNo - 1);
            } else {
                dprintf ("Not removing a CCache diff because there are none.\n");
            }
        } else {
            dprintf ("Not removing a CCache diff because diffs are off.\n");
        }
            
	try {
		sDiffs.pop_back ();
		sNextSeqNo--;
	} catch (...) {
	}
}

void CCIClassicSupport::RemoveDiffsUpTo (
	CCIUInt32		inSeqNo)
{
    if (KeepDiffs ()) {
        if (sFirstSeqNo > inSeqNo) {
            dprintf ("Can't remove diffs below %d because first diff is at %d\n", inSeqNo, sFirstSeqNo);
        } else if (sNextSeqNo < inSeqNo) {
            dprintf ("Can't remove diffs below %d because last diff is at %d\n", inSeqNo, sNextSeqNo);
        } else {
            while (sFirstSeqNo < inSeqNo) {
                DisposeHandle (*(sDiffs.begin ()));
                sDiffs.pop_front ();
                sFirstSeqNo++;
            }
        }
    } else {
        dprintf ("Not removing CCache diffs because none were kept.\n");
    }
}

void CCIClassicSupport::RemoveAllDiffs ()
{
    if (KeepDiffs ()) {
        sDiffs.clear ();
        sFirstSeqNo = sNextSeqNo = 0;
    } else {
        dprintf ("Not removing all CCache diffs because none were kept.\n");
    }
}

Handle CCIClassicSupport::GetAllDiffsSince (
	CCIUInt32	inSeqNo)
{
    if (KeepDiffs ()) {
        if (inSeqNo < sFirstSeqNo) {
            dprintf ("Need to return diffs starting at %d, but the first diff is at %d.\n", inSeqNo, sFirstSeqNo);
            return NULL;
        } else if (inSeqNo > sNextSeqNo) {
            dprintf ("Need to return diffs starting at %d, but the next diff is  %d.\n", inSeqNo, sNextSeqNo);
            return NULL;
        } else {
            UInt32	count = sNextSeqNo - inSeqNo;
            
            CCIHandleBuffer		diffs;
            CCIUInt32	data = getpid ();
            diffs.Put (data);
    
            if (count != 0) {
                dprintf ("Need to return %d diffs starting at %d.\n", count, inSeqNo);
        
                CCIUInt32 seqNo = sFirstSeqNo;
                for (std::list <Handle>::const_iterator i = sDiffs.begin ();
                        i != sDiffs.end ();
                        i++, seqNo++) {
                        
                        if (seqNo >= inSeqNo) {
                                data = ccClassic_DiffCookie;
                                diffs.Put (data);
                                
                                data = seqNo;
                                diffs.Put (data);
                                
                                HLock (*i);
                                diffs.PutData (*(*i), GetHandleSize (*i));
                                HUnlock (*i);
                        }
                }
            }
            
            data = ccClassic_ResponseCookie;
            diffs.Put (data);
            
            Handle allDiffs = diffs.GetHandle ();
            diffs.ReleaseHandle ();
            
            return allDiffs;
        }
    } else {
        dprintf ("Not returning any diffs since none were kept.\n");
        return NULL;
    }
}

Handle CCIClassicSupport::FabricateDiffs ()
{
    CCIUInt32	seqNo = 0;

    CCIContextData*	context = CCIContextDataInterface::GetGlobalContext ();
    std::vector <CCIObjectID>	ccaches;
    context -> GetCCacheIDs (ccaches);
    
    sort (ccaches.begin (), ccaches.end ());
    
    std::vector <CCIObjectID>::iterator	i;
    
    CCIHandleBuffer	diffs;
    
    CCIUInt32	diffType;
    
    CCIUInt32	data = getpid ();
    diffs.Put (data);

    CCIUInt32	diffCookie = ccClassic_DiffCookie;

    for (i = ccaches.begin (); i != ccaches.end (); i++) {
        diffs.Put (diffCookie);
        diffs.Put (seqNo);
        seqNo++;

        diffType = ccClassic_CCache_SkipToID;
        diffs.Put (diffType);
        diffs.Put (*i);
        
        CCICCacheDataInterface	ccache (*i);
        
        diffs.Put (diffCookie);
        diffs.Put (seqNo);
        seqNo++;

        diffType = ccClassic_Context_CreateCCache;
        diffs.Put (diffType);
        diffs.Put (context -> GetGloballyUniqueID ());
        
        CCIUInt32	version = ccache -> GetCredentialsVersion ();
        if ((version & cc_credentials_v4) != 0) {
            diffs.Put (ccache -> GetName ());
            diffs.Put (version & cc_credentials_v4);
            diffs.Put (ccache -> GetPrincipal (cc_credentials_v4));
        } else {
            diffs.Put (ccache -> GetName ());
            diffs.Put (version & cc_credentials_v5);
            diffs.Put (ccache -> GetPrincipal (cc_credentials_v5));
        }
        
        if (version == cc_credentials_v4 | cc_credentials_v5) {
            diffs.Put (diffCookie);
            diffs.Put (seqNo);
            seqNo++;

            diffType = ccClassic_CCache_SetPrincipal;
            diffs.Put (diffType);
            diffs.Put (*i);
            diffs.Put (version & cc_credentials_v5);
            diffs.Put (ccache -> GetPrincipal (cc_credentials_v5));
        }
        
        std::vector <CCIObjectID>	creds;
        ccache -> GetCredentialsIDs (creds);
        
        sort (creds.begin (), creds.end ());
        
        std::vector <CCIObjectID>::iterator j;
        
        for (j = creds.begin (); j != creds.end (); j++) {
            diffs.Put (diffCookie);
            diffs.Put (seqNo);
            seqNo++;

            CCIUInt32 diffType = ccClassic_Credentials_SkipToID;
            diffs.Put (diffType);
            diffs.Put (*j);
            
            CCICredentialsDataInterface	creds (*j);
            
            diffs.Put (diffCookie);
            diffs.Put (seqNo);
            seqNo++;

            diffType = ccClassic_CCache_StoreConvertedCredentials;
            diffs.Put (diffType);
            diffs.Put (*i);
            
            std::strstream		flatCredentials;
            flatCredentials << creds.Get () << std::ends;
            
            diffs.Put (flatCredentials);
            flatCredentials.freeze (false);	// Makes sure the buffer will be deallocated
        }
        
        diffs.Put (diffCookie);
        diffs.Put (seqNo);
        seqNo++;

        diffType = ccClassic_Credentials_SkipToID;
        diffs.Put (diffType);
        diffs.Put (CCIUniqueGlobally <CCICredentialsData>::GetNextGloballyUniqueID ());
    }

    diffs.Put (diffCookie);
    diffs.Put (seqNo);
    seqNo++;

    diffType = ccClassic_CCache_SkipToID;
    diffs.Put (diffType);
    diffs.Put (CCIUniqueGlobally <CCICCacheData>::GetNextGloballyUniqueID ());
    
    Handle diffHandle = diffs.GetHandle ();
    
    CCIUInt32	responseCookie = ccClassic_ResponseCookie;
    diffs.Put (responseCookie);
    CCIUInt32	response = ccNoError;
    diffs.Put (response);

    diffs.ReleaseHandle ();
    
    sKeepDiffs = true;
    
    return diffHandle;
}