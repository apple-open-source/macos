/*
 * Copyright (c) 2000-2001,2011-2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// cssmclient - common client interface to CSSM and MDS.
//
// Locking Strategy (preliminary):
// XXX This is obsolete update this --mb
// A CssmObject is a CountingMutex. Its count represents the number of children that have registered
// themselves (using addChild/removeChild). The lock controls the internal management fields of the
// various subclasses to protect them against corruption. It does NOT control attribute and argument
// fields and operations, not does it control object-constant fields.
// This means that if you use an object from multiple threads, you (the caller) must lock the object
// during set/get calls of attributes. Note that the CSSM operations themselves are safely multithreaded
// and thus don't need to be interlocked explicitly.
//
#include <security_cdsa_client/cssmclient.h>
#include <syslog.h>

using namespace CssmClient;

//
// Exception model
//
const char *
Error::what () const throw()
{
	return "CSSM client library error";
}


//
// General utilities
//
void
ObjectImpl::check(CSSM_RETURN status)
{
	if (status != CSSM_OK)
	{
		CssmError::throwMe(status);
	}
}


//
// Common features of Objects
//
ObjectImpl::ObjectImpl() : mParent(), mChildCount(0)
{
	mActive = false;		// not activated
	mAllocator = NULL;		// allocator to be determined
}

ObjectImpl::ObjectImpl(const Object &mommy) : mParent(mommy.mImpl), mChildCount(0)
{
	mActive = false;		// not activated
	mAllocator = NULL;		// allocator to be determined
	if (mParent)
		mParent->addChild();
}

ObjectImpl::~ObjectImpl()
try
{
    if (!isIdle())
    {
        int i = mChildCount;
        syslog(LOG_ALERT, "Object %p still has %d children at delete.\n", this, i);
    }
		
	// release parent from her obligations (if we still have one)
	if (mParent)
		mParent->removeChild();
}
catch(...)
{
	return;
}

void
ObjectImpl::addChild()
{
	mChildCount++;		// atomic
}

void
ObjectImpl::removeChild()
{
	mChildCount--;		// atomic
}


//
// Manage allocators in the Object tree
//
Allocator &
ObjectImpl::allocator() const
{
	if (mAllocator == NULL)
	{
		// fix allocator now
		if (mParent)
			mAllocator = &mParent->allocator();
		else
			mAllocator = &Allocator::standard();
	}

	return *mAllocator;
}

void
ObjectImpl::allocator(Allocator &alloc)
{
	assert(mAllocator == NULL);	// cannot redefine allocator once set
	mAllocator = &alloc;
}

// Comparison operators use pointer comparison by default.  Subclasses may override.
bool
ObjectImpl::operator <(const ObjectImpl &other) const
{
	return this < &other;
}

bool
ObjectImpl::operator ==(const ObjectImpl &other) const
{
	return this == &other;
}


//
// CSSMSession objects.
// parent ::= NULL (none)
// active ::= CSSM initialized
//
ModuleNexus<CssmImpl::StandardCssm> CssmImpl::mStandard;

CssmImpl::CssmImpl() : ObjectImpl()
{
	setup();
	mStandard().setCssm(this);
}

CssmImpl::CssmImpl(bool) : ObjectImpl()
{
	setup();
	// implicitly constructed - caller responsible for standard session management
}

CssmImpl::~CssmImpl()
{
    try
    {
		deactivate();
    }
	catch(...) {}

	// this may be the standard session...
    mStandard().unsetCssm(this);
}


void
CssmImpl::setup()
{
	// set default configuration
	mVersion.Major = 2;
	mVersion.Minor = 0;
	mScope = CSSM_PRIVILEGE_SCOPE_PROCESS;
}


Cssm
CssmImpl::standard()
{
    return Cssm(mStandard().get());
}


void
CssmImpl::activate()
{
    StLock<Mutex> _(mActivateMutex);
	if (!mActive)
	{
		// currently, no choices on PVC mode and key hierarchy
		CSSM_PVC_MODE pvc = CSSM_PVC_NONE;
		switch (CSSM_RETURN rc = CSSM_Init(&mVersion,
				mScope, &mCallerGuid,
				CSSM_KEY_HIERARCHY_NONE, &pvc, NULL)) {
		case CSSMERR_CSSM_PVC_ALREADY_CONFIGURED:
		case CSSM_OK:
			break;
		default:
			check(rc);
		}
		mActive = true;
	}
}

void
CssmImpl::deactivate()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;

		// clear module map (all gone now)
		moduleMap.erase(moduleMap.begin(), moduleMap.end());
	
		// now terminate CSSM
		check(CSSM_Terminate());
	}
}

void
CssmImpl::atExitHandler()
{
    try {
        mStandard.reset();
    } catch (...) {
    }
}

void
CssmImpl::catchExit()
{
	// @@@ Even though this is the "right thing" to do.  This only causes
	// exceptions during exit and doesn't really help cleanup correctly.
#if 0
	if (::atexit(atExitHandler))
		UnixError::throwMe();
#endif
}


//
// Manage the automatic Cssm object.
// This is a program global.
//
void CssmImpl::StandardCssm::setCssm(CssmImpl *cssm)
{
    StLock<Mutex> _(*this);
    if (mCssm == NULL)
        mCssm = cssm;
}

void CssmImpl::StandardCssm::unsetCssm(CssmImpl *cssm)
{
    StLock<Mutex> _(*this);
    if (mCssm == cssm)
        mCssm = NULL;
}

CssmImpl *CssmImpl::StandardCssm::get()
{
    StLock<Mutex> _(*this);
    if (mCssm == NULL) {	// make the default instance
        mCssm = new CssmImpl(true);
    }
    return mCssm;
}

CssmImpl::StandardCssm::~StandardCssm()
{
    if (mCssm) {
        mCssm->deactivate();
        delete mCssm;
    }
}


//
// Auto-module management
//
Module
CssmImpl::autoModule(const Guid &guid)
{
	StLock<Mutex> _(mapLock);
	ModuleMap::iterator it = moduleMap.find(guid);
	if (it == moduleMap.end())
	{
		// no automodule for this guid yet, create one
		Module module(guid, Cssm(this));
		moduleMap.insert(ModuleMap::value_type(guid, module));
		return module;
	}
	else
	{
		// existing automodule - use it
		return it->second;
	}
}


//
// Module objects.
// parent ::= the session object (usually Cssm::standard)
// active ::= module is loaded.
//
ModuleImpl::ModuleImpl(const Guid &guid) : ObjectImpl(Cssm::standard()),
	mAppNotifyCallback(NULL),
	mAppNotifyCallbackCtx(NULL)
{
	setGuid(guid);
}

ModuleImpl::ModuleImpl(const Guid &guid, const Cssm &session) : ObjectImpl(session),
	mAppNotifyCallback(NULL),
	mAppNotifyCallbackCtx(NULL)
{
	setGuid(guid);
}

ModuleImpl::~ModuleImpl()
{
	unload();
}


//
// RawModuleEvent objects encapsulate CSSM module callbacks
//
RawModuleEvents::~RawModuleEvents()
{ }

CSSM_RETURN RawModuleEvents::sendNotify(const CSSM_GUID *, void *context,
	uint32 subService, CSSM_SERVICE_TYPE type, CSSM_MODULE_EVENT event)
{
	try {
		reinterpret_cast<RawModuleEvents *>(context)->notify(subService, type, event);
		return CSSM_OK;
	} catch (const CommonError &error) {
		return CssmError::cssmError(error, CSSM_CSSM_BASE_ERROR);
	} catch (...) {
		return CSSMERR_CSSM_INTERNAL_ERROR;	// whatever...
	}
}


//
// ModuleEvents enhance RawModuleEvents by splitting the callback up by type
//
void ModuleEvents::notify(uint32 subService,
	CSSM_SERVICE_TYPE type, CSSM_MODULE_EVENT event)
{
	switch (event) {
	case CSSM_NOTIFY_INSERT:
		insertion(subService, type);
		break;
	case CSSM_NOTIFY_REMOVE:
		removal(subService, type);
		break;
	case CSSM_NOTIFY_FAULT:
		fault(subService, type);
		break;
	}
}

// default callbacks do nothing
void ModuleEvents::insertion(uint32 subService, CSSM_SERVICE_TYPE type) { }
void ModuleEvents::removal(uint32 subService, CSSM_SERVICE_TYPE type) { }
void ModuleEvents::fault(uint32 subService, CSSM_SERVICE_TYPE type) { }


void
ModuleImpl::appNotifyCallback(CSSM_API_ModuleEventHandler appNotifyCallback, void *appNotifyCallbackCtx)
{
	secdebug("callback","In ModuleImpl::appNotifyCallback, appNotifyCallback=%p, appNotifyCallbackCtx=%p",
		appNotifyCallback, appNotifyCallbackCtx);
	if (mActive)
		Error::throwMe(Error::objectBusy);

	mAppNotifyCallback = appNotifyCallback;
	mAppNotifyCallbackCtx = appNotifyCallbackCtx;
}

void
ModuleImpl::appNotifyCallback(RawModuleEvents *handler)
{
	appNotifyCallback(RawModuleEvents::sendNotify, handler);
}

void
ModuleImpl::activate()
{
    {
        StLock<Mutex> _(mActivateMutex);
        if (!mActive)
        {
            session()->init();
            // @@@ install handler here (use central dispatch with override)
            secdebug("callback","In ModuleImpl::activate, mAppNotifyCallback=%p, mAppNotifyCallbackCtx=%p",
                mAppNotifyCallback, mAppNotifyCallbackCtx);
            check(CSSM_ModuleLoad(&guid(), CSSM_KEY_HIERARCHY_NONE, mAppNotifyCallback, mAppNotifyCallbackCtx));
            mActive = true;
        }
    }
    
    session()->catchExit();
}

void
ModuleImpl::deactivate()
{
	if (!isIdle())
		Error::throwMe(Error::objectBusy);

    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;
		check(CSSM_ModuleUnload(&guid(), mAppNotifyCallback, mAppNotifyCallbackCtx));
	}
}

Cssm
ModuleImpl::session() const
{
	return parent<Cssm>();
}


//
// CssmAttachment objects.
// parent ::= the loaded module object.
// active ::= attached.
//
AttachmentImpl::AttachmentImpl(const Guid &guid, CSSM_SERVICE_TYPE subserviceType)
: ObjectImpl(CssmImpl::standard()->autoModule(guid))
{
	make(subserviceType);
}

AttachmentImpl::AttachmentImpl(const Module &module, CSSM_SERVICE_TYPE subserviceType)
: ObjectImpl(module)
{
	make(subserviceType);
}

AttachmentImpl::~AttachmentImpl()
{
	detach();
}

void
AttachmentImpl::make(CSSM_SERVICE_TYPE subserviceType)
{
	// default configuration
	mVersion.Major = 2;
	mVersion.Minor = 0;
	mSubserviceType = subserviceType;
	mSubserviceId = 0;
	mAttachFlags = 0;
}

void
AttachmentImpl::activate()
{
    StLock<Mutex> _(mActivateMutex);
	if (!mActive)
	{
		module()->load();
		mMemoryFunctions = CssmAllocatorMemoryFunctions(allocator());
		check(CSSM_ModuleAttach(&guid(), &mVersion,
			  &mMemoryFunctions,
			  mSubserviceId,
			  mSubserviceType,
			  mAttachFlags,
			  CSSM_KEY_HIERARCHY_NONE,
			  NULL, 0,	// no function pointer table return
			  NULL,		// reserved
			  &mHandle));
		mActive = true;
	}
}

void
AttachmentImpl::deactivate()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;
		check(CSSM_ModuleDetach(mHandle));
	}
}

CSSM_SERVICE_MASK
AttachmentImpl::subserviceMask() const
{
	return mSubserviceType;
}

void
AttachmentImpl::subserviceId(uint32 id)
{
	mSubserviceId = id;
}

CssmSubserviceUid
AttachmentImpl::subserviceUid() const
{
	return CssmSubserviceUid(guid(), &mVersion, mSubserviceId, subserviceMask());
}

Module
AttachmentImpl::module() const
{
	return parent<Module>();
}
