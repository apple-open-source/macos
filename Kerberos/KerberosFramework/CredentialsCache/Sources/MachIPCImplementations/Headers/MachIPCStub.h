#ifndef CCIMachIPCStub_h
#define CCIMachIPCStub_h

#include <pthread.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/mach_init.h>
#include <Kerberos/mach_client_utilities.h>

#pragma once

/*
The Client Change Time:

Because servers launch and quit constantly (as tickets are created and 
destroyed) during the lifetime of a client, we need to maintain the client's 
idea of the change time.  This time is always returned when the client 
requests the change time and is updated from the server change time when a 
server is available.  

The client change time allows us to guarantee the following two properties:

(1) Each client sees a monotonically increasing change time.
(2) If a change occurs between change time cA and change time cB, cA < cB.

There are five cases we need to handle for each pair of change time requests.  
For the purposes of this discussion, the requests are called rA and rB, the 
server change times (if the server is running) are sA and sB, and the change 
times returned to the caller are cA and cB.

(1) No server is running for rA and no server is running for rB.

    Since there is no change, cB == cA.

(2) Server X is running for rA and server X is running for rB.

    If sB > sA, we must guarantee that cB > cA.  Otherwise cB == cA.

(3) No server is running for rA and server X is running for rB.
    
    The client has a change time cA. Now we must guarantee that cB > cA 
    because a server launched and the state of ticket cache has changed.  
    
    So what we do is when we notice the server for the first time, is 
    if cA >= sB (the client change time is ahead of the server), we 
    generate client side offset cO = cA - sB + 1.  Otherwise we just use 
    the server change time (cO = 0).  
    
    While the server is running, we return the server change time plus the 
    offset (eg: cB = sB + cO) to the caller.  The reason we can't just add 
    the offset is because then we would just get screwed on the subsequent 
    request rC if cB >= sC (we have to maintain case (2)).

(4) Server X is running for rA and no server is running for rB.

    Again the state has changed so we need cB > cA.  
    
    This is easy to implement because we just set cB to the maximum of
    cA + 1 or the current time. In order to avoid complications 
    surrounding the offset, we actually set 
    cB = max (cA + cO + 1, time (NULL)) and then set cO = 0.  This means 
    that the next time case (3) happens, we don't have to handle when 
    cO > 0.

(5) Server X is running for rA and server Y is running for rB.

    This is the pessimal case.  The client completely misses 
    server X quitting and a new server Y launching.  In this case, sA and 
    sB are not from the same server, and thus server Y doesn't know the 
    value of sA and doesn't guarantee that sB >= sA.  So we need to enforce 
    this in the client. 
    
    Now obviously if sB < sA, then we know we have a new server. So we can 
    just increment the offset cO and force the client to re-check.  But if 
    sB == sA, we can't tell the different between case 2 when there are no 
    changes and case 5.
    
    If we want to support case 5 for a client who opens a new ccache 
    context for every call to get the change time, we would have to implement 
    a shared Mach port for all ccache contexts so we could track the port 
    becoming invalid between contexts.  This would be horrible from a thread 
    safety standpoint because we would have multiple threads invalidating the 
    port, launching the server, etc.  I believe we very much don't want to 
    try to maintain this state between threads.  
    
    Instead, we will require that either the client always use the same 
    context to get the change time, or in case 5 we will only guarantee 
    that cB >= cA even though cB > cA is strictly correct.
    
    Here are the circumstances that produce cB == cA when it should be cB > cA:
        
    * The client uses separate contexts for each call to get the change time.
        
    * Between the two calls to get the change time, the current server quits
      and a new server launches.  
        
    * The change times returned from each server are identical.  This basically
      means that the client change time or the first server's change time has
      to have gotten ahead of the current time by at least a second.  The new
      server will only launch if someone is storing tickets, so its 
      change time must be at least the current time in UTC when it launched.
        
    * The client makes no other ccache calls while there is no server running
      (or the client would notice there is no server).
        
    Because this case is extremely unlikely, I believe it is acceptable to
    return cB == cA in this case.  KLLastChangeTime now uses a single context
    which should catch most clients.
    
    We will document that callers should use the same context for two requests
    for the cache change time if the caller wants to compare those two times.

*/

class CCIChangeTimeStub {
    public:
        CCIChangeTimeStub () : 
            mServerRunning (false), mServerTimeOffset (0), mChangeTime (0) {
                int err = pthread_mutex_init (&mMutex, NULL);
                if (err == ENOMEM) {
                    throw CCIException (ccErrNoMem);
                } else if (err != 0) {
                    throw CCIException (ccErrBadParam);
                }
            }
        
        virtual ~CCIChangeTimeStub () {
            pthread_mutex_destroy (&mMutex);
        }
        
        CCITime Get () { 
            pthread_mutex_lock (&mMutex);
            CCITime time = mChangeTime + mServerTimeOffset;
            pthread_mutex_unlock (&mMutex);
            return time; 
        }
        
        void UpdateFromServer (CCITime newTime) { 
            pthread_mutex_lock (&mMutex);
            
            if (!mServerRunning) {
                // Process got change time from the server for the first time or  
                // since we had it die.  Remember we've seen the server:
                mServerRunning = true;
                
                // Now check to make sure the server time is greater than fake change time
                // generated by the client (case 3)
                if (newTime < (mChangeTime + mServerTimeOffset)) {
                    // Uh oh... we need the client to talk to the server because we just 
                    // noticed it. But the server time isn't greater than the fake time 
                    // we made up when we hadn't contacted it yet.  
                    // So create an offset from the time which will make it bigger.
                    // We use an offset so the server time is still monotonically increasing.
                    mServerTimeOffset += mChangeTime - newTime + 1;
                }
            } else if (newTime < mChangeTime) {
                // Safety check for case 5: If this happens we know we are talking to
                // a different server because the same server wouldn't have given us this
                // bogus new time. Increase the offset so the client checks the ccache:
                mServerTimeOffset++;
            }
            
            if (mChangeTime < newTime) {
                // only update the change time if it makes it bigger
                // so we guarantee monotonic increases
                mChangeTime = newTime;
            }
            pthread_mutex_unlock (&mMutex);
        }
        
        void UpdateWhenServerDies () { 
            pthread_mutex_lock (&mMutex);
            
            // Reset the offset and remember the server died (case 4)
            mChangeTime += mServerTimeOffset;
            mServerTimeOffset = 0;
            mServerRunning = false;
            
            // Now tell the client the server changed with our update time
            CCITime now = static_cast <CCITime> (time (NULL));
            mChangeTime = (mChangeTime < now) ? now : mChangeTime + 1;
            
            pthread_mutex_unlock (&mMutex);
        }
        
    private:
        pthread_mutex_t	mMutex;
        bool			mServerRunning;
        CCITime 		mServerTimeOffset;
        CCITime 		mChangeTime;
};

class CCIMachIPCStub {
    public:
        CCIMachIPCStub ();
        virtual ~CCIMachIPCStub ();
            
        virtual mach_port_t GetPort () const;
        
        virtual mach_port_t GetPortNoLaunch () const;

        void InvalidatePort () const;
        
        static void UpdateServerPortState (mach_port_t newServerPort);
        
        static CCITime GetServerStateChangedTime ();
        
        void UpdateStateChangedTimeFromServer (CCITime newTime);

        static mach_port_t GetLastServerPort ();
        
    private:
        // mutable so GetPort and GetPortNoLaunch can modify them
        mutable MachServerPort 		*mPort;
        static bool					sSeenServerPort;
        static mach_port_t			sLastServerPort;
        static CCIChangeTimeStub	sServerStateChangedTime;
};

template <class T>
class CCIMachIPCBuffer {
    public:
        CCIMachIPCBuffer ():
            mData (NULL),
            mSize (0)
        {
        }
        
        T*& Data () { return mData; }
        mach_msg_type_number_t& Size () { return mSize; }
        mach_msg_type_number_t Count () const { return mSize / sizeof (T); }
        
        ~CCIMachIPCBuffer () {
            if (mData != NULL) {
                (void) vm_deallocate (mach_task_self (), (vm_offset_t) mData, mSize * sizeof (T));
            }
        }
    
    private:
        CCIMachIPCBuffer (const CCIMachIPCBuffer&);
        CCIMachIPCBuffer& operator= (const CCIMachIPCBuffer&);
        
        T*			mData;
        mach_msg_type_number_t	mSize;
};

#endif // CCIMachIPCStub_h