/*
 *  agentclient.cpp
 *  SecurityAgent
 *
 *  Copyright (c) 2002,2008,2011-2012 Apple Inc.. All Rights Reserved.
 *
 */


#include <stdio.h>

/*
For now all the calls into agentclient will be synchronous, with timeouts

On a timeout, we will return control to the client, but we really need to send the appropriate abort right there and then, otherwise they'll need to call the same method again to check that the reply still isn't there.

If we receive a reply that is not confirming attempts to abort, we'll process these and return them to the caller.

Alternatively, there can be an answer that isn't the answer we expected: setError, where the server aborts the transaction.

We can't support interrupt() with a synchronous interface unless we add some notification that let's the client know that the "server" is dead
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <grp.h>

#include <security_agent_server/sa_reply.h> // for size of replies
#include <security_agent_client/sa_request.h>

#include <security_utilities/mach++.h>
#include <security_cdsa_utilities/walkers.h>
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/AuthorizationWalkers.h>
#include <security_cdsa_utilities/AuthorizationData.h>

#include <security_utilities/logging.h>

using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;
using Security::DataWalkers::walk;

using Authorization::AuthItemSet;
using Authorization::AuthItemRef;
using Authorization::AuthValueOverlay;

#include "agentclient.h"

namespace SecurityAgent {
    
class CheckingReconstituteWalker {
public:
    CheckingReconstituteWalker(void *ptr, void *base, size_t size)
    : mBase(base), mLimit(increment(base, size)), mOffset(difference(ptr, base)) { }
	
	template <class T>
	void operator () (T &obj, size_t size = sizeof(T))
{ }
	
    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
{
		blob(addr, size);
}

template <class T>
void blob(T * &addr, size_t size)
{
	DEBUGWALK("checkreconst:*");
	if (addr) {
		if (addr < mBase || increment(addr, size) > mLimit)
			MacOSError::throwMe(errAuthorizationInternal);
		addr = increment<T>(addr, mOffset);
	}
}

static const bool needsRelinking = true;
static const bool needsSize = false;

private:
void *mBase;			// old base address
void *mLimit;			// old last byte address + 1
off_t mOffset;			// relocation offset
};

template <class T>
void relocate(T *obj, T *base, size_t size)
{
    if (obj) {
        CheckingReconstituteWalker w(obj, base, size);
        walk(w, base);
    }
}

void Client::check(mach_msg_return_t returnCode)
{
	// first check the Mach IPC return code
	switch (returnCode) {
		case MACH_MSG_SUCCESS:				// peachy
			break;
		case MIG_SERVER_DIED:			// explicit can't-send-it's-dead
			CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
		default:						// some random Mach error
			MachPlusPlus::Error::throwMe(returnCode);
	}
}

void Client::checkResult()
{
        // now check the OSStatus return from the server side
        switch (result()) {
            case kAuthorizationResultAllow: return;
            case kAuthorizationResultDeny:
            case kAuthorizationResultUserCanceled: CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
            default: MacOSError::throwMe(errAuthorizationInternal);
        }
}



#pragma mark administrative operations

Client::Client() : mState(init), mTerminateOnSleep(false)
{
	// create reply port
	mClientPort.allocate(); //implicit MACH_PORT_RIGHT_RECEIVE
		
	// register with agentclients
	Clients::gClients().insert(this);
    
    // add this client to the global list so that we can kill it if we have to
    StLock<Mutex> _(Clients::gAllClientsMutex());
    Clients::allClients().insert(this);
}

void
Client::activate(Port serverPort)
{
	if (!serverPort)
		MacOSError::throwMe(errAuthorizationInternal);

	secdebug("agentclient", "using server at port %d", serverPort.port());
	mServerPort = serverPort;
}


Client::~Client()
{
    teardown();
}


// start/endTransaction calls stand outside the usual client protocol: they
// don't participate in the state-management or multiplexing-by-port tangoes.  
// (These calls could take advantage of activate(), but callers would be 
// instantiating an entire Client object for the sake of mServerPort.)
// Conversely, SecurityAgent::Client does not cache transaction state.  
OSStatus
Client::startTransaction(Port serverPort)
{
    if (!serverPort)
        return errAuthorizationInternal;
    kern_return_t ret = sa_request_client_txStart(serverPort);
    secdebug("agentclient", "Transaction started (port %u)", serverPort.port());
    return ret;
}

OSStatus 
Client::endTransaction(Port serverPort)
{
    if (!serverPort)
        return errAuthorizationInternal;
    secdebug("agentclient", "Requesting end of transaction (port %u)", serverPort.port());
    return sa_request_client_txEnd(serverPort);
}

void 
Client::setState(PluginState inState)
{
	// validate state transition: might be more useful where change is requested if that implies anything to interpreting what to do.
	// Mutex
	mState = inState;
}

void Client::teardown() throw()
{
    // remove this from the global list
    {
        StLock<Mutex> _(Clients::gAllClientsMutex());
        Clients::allClients().erase(this);
    }

	Clients::gClients().remove(this);

	try {
		if (mStagePort)
			mStagePort.destroy();
		if (mClientPort)
			mClientPort.destroy();
	} catch (...) { secdebug("agentclient", "ignoring problems tearing down ports for client %p", this); }
}


AuthItemSet
Client::clientHints(SecurityAgent::RequestorType type, std::string &path, pid_t clientPid, uid_t clientUid)
{
    AuthItemSet clientHints;
	
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_TYPE, AuthValueOverlay(sizeof(type), &type)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PATH, AuthValueOverlay(path)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PID, AuthValueOverlay(sizeof(clientPid), &clientPid)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_UID, AuthValueOverlay(sizeof(clientUid), &clientUid)));

    return clientHints;
}


#pragma mark request operations

OSStatus Client::contact(mach_port_t jobId, Bootstrap processBootstrap, mach_port_t userPrefs)
{
    kern_return_t ret = sa_request_client_contact(mServerPort, mClientPort, jobId, processBootstrap, userPrefs);
    if (ret)
    {
        Syslog::error("SecurityAgent::Client::contact(): kern_return error %s", 
                      mach_error_string(ret));
    }
    return ret;
}

OSStatus Client::create(const char *inPluginId, const char *inMechanismId, const SessionId inSessionId)
{
	// securityd is already notified when the agent/authhost dies through SIGCHILD, and we only really care about the stage port, but we will track if dying happens during create with a DPN.  If two threads are both in the time between the create message and the didcreate answer and the host dies, one will be stuck - too risky and I will win the lottery before that: Chablis.
	{
		kern_return_t ret;
		mach_port_t old_port = MACH_PORT_NULL;
		ret = mach_port_request_notification(mach_task_self(), mServerPort, MACH_NOTIFY_DEAD_NAME, 0, mClientPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &old_port);
		if (!ret && (MACH_PORT_NULL != old_port))
			mach_port_deallocate(mach_task_self(), old_port);
	}

	secdebug("agentclient", "asking server at port %d to create %s:%s; replies to %d", mServerPort.port(), inPluginId, inMechanismId, mClientPort.port()); // XXX/cs
	kern_return_t ret = sa_request_client_create(mServerPort, mClientPort, inSessionId, inPluginId, inMechanismId);

	if (ret)
		return ret;
	
	// wait for message (either didCreate, reportError)
	do
	{
		// one scenario that could happen here (and only here) is:
		// host died before create finished - in which case we'll get a DPN
		try { receive(); } catch (...) { setState(dead); }
	}
	while ((state() != created) && 
			(state() != dead));
	
    // verify that we got didCreate
	if (state() == created)
		return noErr;
	
    // and not reportError
	if (state() == dead)
		return Client::getError();

	// something we don't deal with
	secdebug("agentclient", "we got an error on create"); // XXX/cs
    return errAuthorizationInternal;
}

// client maintains their own copy of the current data
OSStatus Client::invoke()
{
	if ((state() != created) &&
        (state() != active) &&
        (state() != interrupting))
		return errAuthorizationInternal;
	
    AuthorizationValueVector *arguments;
    AuthorizationItemSet *hints, *context;
    size_t argumentSize, hintSize, contextSize;

    mInHints.copy(hints, hintSize);
    mInContext.copy(context, contextSize);
    mArguments.copy(&arguments, &argumentSize);
        
    setState(current);
    
    check(sa_request_client_invoke(mStagePort.port(), 
                    arguments, argumentSize, arguments, // data, size, offset
                    hints, hintSize, hints,
                    context, contextSize, context));
	
    receive();
    
	free (arguments);
	free (hints);
	free (context);
	
    switch(state())
    {
        case active: 
            switch(result())
            {
                case kAuthorizationResultUndefined:
                    MacOSError::throwMe(errAuthorizationInternal);
                default: 
                    return noErr;
            }
        case dead: 
            return mErrorState;
        case current:
            return noErr;
        default:
            break;
    }
    return errAuthorizationInternal;
}

OSStatus
Client::deactivate()
{
	// check state is current
	if (state() != current)
		return errAuthorizationInternal;
	
	secdebug("agentclient", "deactivating mechanism at request port %d", mStagePort.port());

	// tell mechanism to deactivate
	check(sa_request_client_deactivate(mStagePort.port()));
	
	setState(deactivating);
	
	receive(); 
	
	// if failed destroy it
	return noErr;
}

OSStatus
Client::destroy()
{
	if (state() == active || state() == created || state() == current)
	{
		secdebug("agentclient", "destroying mechanism at request port %d", mStagePort.port());
		// tell mechanism to destroy
		if (mStagePort)
			sa_request_client_destroy(mStagePort.port());
		
		setState(dead);
	
		return noErr;
	}
	
	return errAuthorizationInternal;
}

// kill host: do not pass go, do not collect $200
OSStatus
Client::terminate()
{
	check(sa_request_client_terminate(mServerPort.port()));
	
    return noErr;
}

void
Client::receive()
{
    bool gotReply = false;
    while (!gotReply)
	gotReply = Clients::gClients().receive();
}

#pragma mark result operations

void Client::setResult(const AuthorizationResult inResult, const AuthorizationItemSet *inHints, const AuthorizationItemSet *inContext)
{
    if (state() != current)
		return;
    // construct AuthItemSet for hints and context (deep copy - previous contents are released)
	mOutHints = (*inHints);
	mOutContext = (*inContext);
    mResult = inResult;
    setState(active);
}

void Client::setError(const OSStatus inMechanismError)
{
	setState(dead);

    mErrorState = inMechanismError; 
}

OSStatus Client::getError()
{
    return mErrorState;
}

void Client::requestInterrupt()
{
	if (state() != active)
		return;

	setState(interrupting);
}

void Client::didDeactivate()
{
	if (state() != deactivating)
		return;

	// check state for deactivating
	// change state
	setState(active);
}

void Client::setStagePort(const mach_port_t inStagePort)
{
    mStagePort = Port(inStagePort); 
    mStagePort.requestNotify(mClientPort, MACH_NOTIFY_DEAD_NAME, 0);
}


void Client::didCreate(const mach_port_t inStagePort)
{
	// it can be dead, because the host died, we'll always try to revive it once
	if ((state() != init) && (state() != dead))
		return;

	setStagePort(inStagePort);
	setState(created);
}

#pragma mark client instances

ThreadNexus<Clients> Clients::gClients;
ModuleNexus<set<Client*> > Clients::allClients;

bool
Clients::compare(const Client * client, mach_port_t instance)
{
    if (client->instance() == instance) return true;
    return false;
}

// throw so the agent client operation is aborted
Client&
Clients::find(mach_port_t instanceReplyPort) const
{
	StLock<Mutex> _(mLock);
    for (set<Client*>::const_iterator foundClient = mClients.begin(); 
        foundClient != mClients.end();
        foundClient++)
        {
            Client *client = *foundClient;
            if (client->instance() == instanceReplyPort)
                return *client;
        }
        
    // can't be receiving for a client we didn't create
	MacOSError::throwMe(errAuthorizationInternal);
}

bool
Clients::receive()
{
	try 
	{
		// maximum known message size (variable sized elements are already forced OOL)
		Message in(sizeof(union __ReplyUnion__sa_reply_client_secagentreply_subsystem));
		Message out(sizeof(union __ReplyUnion__sa_reply_client_secagentreply_subsystem));

		in.receive(mClientPortSet, 0, 0);

        // got the message, now demux it; call secagentreply_server to handle any call
        // this is asynchronous, so no reply message, although not apparent
        if (!::secagentreply_server(in, out))
        {
    		// port death notification
            if (MACH_NOTIFY_DEAD_NAME == in.msgId())
            {
                find(in.remotePort()).setError(errAuthorizationInternal);
                return true;
            }
	    return false;

        }
	else
		return true;
    } 
    catch (Security::MachPlusPlus::Error &e) 
    {
        secdebug("agentclient", "interpret error %ul", e.error);
        switch (e.error) {
			case MACH_MSG_SUCCESS:				// peachy
				break;
			case MIG_SERVER_DIED:			// explicit can't-send-it's-dead
				CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
			default:						// some random Mach error
				MachPlusPlus::Error::throwMe(e.error);
		}
    }
    catch (...)
    {
        MacOSError::throwMe(errAuthorizationInternal);
    }
       return false; 
}

ModuleNexus<RecursiveMutex> Clients::gAllClientsMutex;

void
Clients::killAllClients()
{
    // grab the lock for the client list -- we need to make sure no one modifies the structure while we are iterating it.
    StLock<Mutex> _(gAllClientsMutex());
    
    set<Client*>::iterator clientIterator = allClients().begin();
    while (clientIterator != allClients().end())
    {
        set<Client*>::iterator thisClient = clientIterator++;
        if ((*thisClient)->getTerminateOnSleep())
        {
            (*thisClient)->terminate();
        }
    }
}



} /* end namesapce SecurityAgent */

#pragma mark demux requests replies
// external C symbols for the mig message handling code to call into

#define COPY_IN(type,name)	type *name, mach_msg_type_number_t name##Length, type *name##Base

// callbacks that key off instanceReplyPort to find the right agentclient instance
// to deliver the message to.

// they make the data readable to the receiver (relocate internal references)
	
kern_return_t sa_reply_server_didCreate(mach_port_t instanceReplyPort, mach_port_t instanceRequestPort)
{
	secdebug("agentclient", "got didCreate at port %u; requests go to port %u", instanceReplyPort, instanceRequestPort);
	SecurityAgent::Clients::gClients().find(instanceReplyPort).didCreate(instanceRequestPort);
	return KERN_SUCCESS;
}

kern_return_t sa_reply_server_setResult(mach_port_t instanceReplyPort, AuthorizationResult result,
	COPY_IN(AuthorizationItemSet,inHints) ,
	COPY_IN(AuthorizationItemSet,inContext) )
{
	secdebug("agentclient", "got setResult at port %u; result %u", instanceReplyPort, (unsigned int)result);

	// relink internal references according to current place in memory
	try { SecurityAgent::relocate(inHints, inHintsBase, inHintsLength); }
	catch (MacOSError &e) {	return e.osStatus(); }
	catch (...) { return errAuthorizationInternal; }

	try { SecurityAgent::relocate(inContext, inContextBase, inContextLength); }
	catch (MacOSError &e) {	return e.osStatus(); }
	catch (...) { return errAuthorizationInternal; }

	SecurityAgent::Clients::gClients().find(instanceReplyPort).setResult(result, inHints, inContext);
			
	return KERN_SUCCESS;
}

kern_return_t sa_reply_server_requestInterrupt(mach_port_t instanceReplyPort)
{
	secdebug("agentclient", "got requestInterrupt at port %u", instanceReplyPort);
	SecurityAgent::Clients::gClients().find(instanceReplyPort).requestInterrupt();
	return KERN_SUCCESS;
}

kern_return_t sa_reply_server_didDeactivate(mach_port_t instanceReplyPort)
{
	secdebug("agentclient", "got didDeactivate at port %u", instanceReplyPort);
	SecurityAgent::Clients::gClients().find(instanceReplyPort).didDeactivate();
	return KERN_SUCCESS;
}

kern_return_t sa_reply_server_reportError(mach_port_t instanceReplyPort, OSStatus status)
{
	secdebug("agentclient", "got reportError at port %u; error is %u", instanceReplyPort, (unsigned int)status);
	SecurityAgent::Clients::gClients().find(instanceReplyPort).setError(status);
	return KERN_SUCCESS;
}

kern_return_t sa_reply_server_didStartTx(mach_port_t replyPort, kern_return_t retval)
{
    // no instance ports here: this goes straight to server
    secdebug("agentclient", "got didStartTx");
    return retval;
}
