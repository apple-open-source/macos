/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// cssmclient - common client interface to CSSM and MDS
//
#ifndef _H_CDSA_CLIENT_CSSMCLIENT
#define _H_CDSA_CLIENT_CSSMCLIENT  1

#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <map>

namespace Security {
namespace CssmClient {


//
// Forward declarations
//
class Cssm;
class Module;
class Attachment;


//
// An mixin for objects that have (store) GUIDs.
// The GUID value is meant to be set-once constant, and can be lock-handled accordingly.
//
class HasGuid {
public:
	HasGuid(const Guid &guid) { mGuid = guid; }
	HasGuid() { }
	
	const Guid &guid() const { return mGuid; }

protected:
	void setGuid(const Guid &guid) { mGuid = guid; }
	
private:
	Guid mGuid;
};


//
// Exceptions are based on the CssmError utility class. We add our own class of client-side exceptions.
//
class Error : public CssmError {
public:
	Error(CSSM_RETURN err) : CssmError(err) { }
	virtual const char *what () const throw();
	
	enum {
		objectBusy = -1,
	};
};


//
// The CssmObject abstract class models features common to different Cssm objects.
// It handles a tree hierarchy of objects (parent/children) safely.
//
class Object;

class ObjectImpl : virtual public RefCount
{
public:
	explicit ObjectImpl(); // Constructor for Impl objects without a parent.
	explicit ObjectImpl(const Object &parent);
	virtual ~ObjectImpl();

	bool isActive() const { return mActive; }

	virtual Allocator &allocator() const;
	virtual void allocator(Allocator &alloc);

	// Pointer comparison by default.  Subclasses may override.
	virtual bool operator <(const ObjectImpl &other) const;
	virtual bool operator ==(const ObjectImpl &other) const;

	static void check(CSSM_RETURN status);

protected:
	bool mActive;					// loaded, attached, etc.
	mutable Allocator *mAllocator; // allocator hierarchy (NULL => TBD)
	
	template <class Obj> Obj parent() const
	{ assert(mParent); return Obj(static_cast<typename Obj::Impl *>(&(*mParent))); }

	void addChild();
	void removeChild();
	bool isIdle() const { return mChildCount == 0; }

	// {de,}activate() assume you have locked *this
	virtual void activate() = 0;
	virtual void deactivate() = 0;

private:
	RefPointer<ObjectImpl> mParent;		// parent object
	AtomicCounter<uint32> mChildCount;
};


class Object
{
	friend class ObjectImpl;
public:
	typedef ObjectImpl Impl;
	explicit Object(Impl *impl) : mImpl(impl) {}

protected:
	// @@@ CSPDL subclass breaks if the is a static_cast
	template <class _Impl> _Impl &impl() const
	{ return dynamic_cast<_Impl &>(*mImpl); }

public:
	Impl *operator ->() const { return &(*mImpl); }
	Impl &operator *() const { return *mImpl; }

	// @@@ Why is this needed.  DbCursor which inheirits from Object wants to call this.
	template <class _Impl> _Impl &checkedImpl() const
	{ return dynamic_cast<_Impl &>(*mImpl); }

	bool operator !() const { return !mImpl; }
	operator bool() const { return mImpl; }
    
    bool isActive() const				{ return mImpl && mImpl->isActive(); }
    Allocator &allocator() const	{ return mImpl->allocator(); }
	void release()						{ mImpl = NULL; }

	bool operator <(const Object &other) const
	{ return mImpl && other.mImpl ? *mImpl < *other.mImpl : mImpl < other.mImpl; }
	bool operator ==(const Object &other) const
	{ return mImpl && other.mImpl ? *mImpl == *other.mImpl : mImpl == other.mImpl; }

private:
	RefPointer<Impl> mImpl;
};


//
// Event callback mix-in class
//
class ModuleImpl;

class RawModuleEvents {
	friend class ModuleImpl;
public:
	virtual ~RawModuleEvents();

	virtual void notify(uint32 subService,
		CSSM_SERVICE_TYPE type, CSSM_MODULE_EVENT event) = 0;

private:
	static CSSM_RETURN sendNotify(const CSSM_GUID *, void *context, uint32 subService,
		CSSM_SERVICE_TYPE type, CSSM_MODULE_EVENT event);
};

class ModuleEvents : public RawModuleEvents {
public:
	virtual void insertion(uint32 subService, CSSM_SERVICE_TYPE type);
	virtual void removal(uint32 subService, CSSM_SERVICE_TYPE type);
	virtual void fault(uint32 subService, CSSM_SERVICE_TYPE type);

protected:
	void notify(uint32 subService, CSSM_SERVICE_TYPE type, CSSM_MODULE_EVENT event);
};


//
// A CSSM loadable module.
// You rarely directly interact with these objects, but if you need to,
// here they are.
//
class ModuleImpl : public ObjectImpl, public HasGuid
{
public:
	ModuleImpl(const Guid &guid);
	ModuleImpl(const Guid &guid, const Cssm &session);
	virtual ~ModuleImpl();
	
	void load() { activate(); }
	void unload() { deactivate(); }
	bool isLoaded() const { return isActive(); }
	
	Cssm session() const;

	void appNotifyCallback(CSSM_API_ModuleEventHandler appNotifyCallback, void *appNotifyCallbackCtx);
	void appNotifyCallback(RawModuleEvents *handler);

protected:
	void activate();
	void deactivate();

	CSSM_API_ModuleEventHandler mAppNotifyCallback;
	void *mAppNotifyCallbackCtx;
};

class Module : public Object
{
public:
	typedef ModuleImpl Impl;
	explicit Module(Impl *impl) : Object(impl) {}
	Module() : Object(NULL) {} // XXX This might break operator <
	Module(const Guid &guid) : Object(new Impl(guid)) {}
	Module(const Guid &guid, const Cssm &session) : Object(new Impl(guid, session)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// An Attachment object. This is the base class of all typed attachment classes.
//
class AttachmentImpl : public ObjectImpl
{
public:
	AttachmentImpl(const Guid &guid, CSSM_SERVICE_TYPE subserviceType);
	AttachmentImpl(const Module &module, CSSM_SERVICE_TYPE subserviceType);
	//AttachmentImpl(... mds reference ...);
	virtual ~AttachmentImpl();

	// Virtual so that subclasses can return there true mask.
	virtual CSSM_SERVICE_MASK subserviceMask() const;

	CSSM_SERVICE_TYPE subserviceType() const { return mSubserviceType; }
	CSSM_VERSION version() const { return mVersion; }
	void version(const CSSM_VERSION &v) { mVersion = v; }
	uint32 subserviceId() const { return mSubserviceId; }
	virtual void subserviceId(uint32 id);
	CSSM_ATTACH_FLAGS flags() const { return mAttachFlags; }
	void flags(CSSM_ATTACH_FLAGS f) { mAttachFlags = f; }

	void attach() { activate(); }
	void detach() { deactivate(); }
	bool attached() const { return isActive(); }

	Module module() const;
	const Guid &guid() const { return module()->guid(); }
	CSSM_MODULE_HANDLE handle() { attach(); return mHandle; }

	CssmSubserviceUid subserviceUid() const;

protected:
	void activate();
	void deactivate();

private:
	void make(CSSM_SERVICE_TYPE subserviceType);	// common constructor

	CSSM_MODULE_HANDLE mHandle;

	CSSM_SERVICE_TYPE mSubserviceType;				// set by constructor
	CSSM_VERSION mVersion;
	uint32 mSubserviceId;
	CSSM_ATTACH_FLAGS mAttachFlags;

	CssmAllocatorMemoryFunctions mMemoryFunctions;	// set on attach()
};

class Attachment : public Object
{
public:
	typedef AttachmentImpl Impl;
	explicit Attachment(Impl *impl) : Object(impl) {}
	Attachment(const Guid &guid, CSSM_SERVICE_TYPE subserviceType)
	: Object(new Impl(guid, subserviceType)) {}
	Attachment(const Module &module, CSSM_SERVICE_TYPE subserviceType)
	: Object(new Impl(module, subserviceType)) {}
	//Attachment(... mds reference ...);

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// A CSSM session object.
// You usually only have one per program, or library, or what-not.
//
class Cssm;

class CssmImpl : public ObjectImpl {
    class StandardCssm; friend class StandardCssm;
public:
	CssmImpl();
	virtual ~CssmImpl();

	void init() { activate(); }
	void terminate() { deactivate(); }

	CSSM_PRIVILEGE_SCOPE scope() const { return mScope; }
	void scope(CSSM_PRIVILEGE_SCOPE sc) { mScope = sc; }
	const Guid &callerGuid() const { return mCallerGuid; }
	void callerGuid(const CSSM_GUID &guid) { mCallerGuid = Guid::overlay(guid); }

	Module autoModule(const Guid &guid);
    
protected:
	explicit CssmImpl(bool);				// internal constructor

	void setup();							// constructor setup

	void activate();
	void deactivate();

private:
	// CSSM global configuration -- picked up on each Init
	CSSM_VERSION mVersion;
	CSSM_PRIVILEGE_SCOPE mScope;
	Guid mCallerGuid;

	// module repository: modules by guid (protected by self)
	typedef map<Guid, Module> ModuleMap;
	ModuleMap moduleMap;
	Mutex mapLock;

public:
	static Cssm standard();
	static void catchExit();

private:
	static void atExitHandler();
    
    class StandardCssm : public Mutex {
    public:
        StandardCssm() : mCssm(NULL) { }
        ~StandardCssm();
        void setCssm(CssmImpl *cssm);
        void unsetCssm(CssmImpl *cssm);
        CssmImpl *get();
        
    private:
        CssmImpl *mCssm;
    };
    static ModuleNexus<StandardCssm> mStandard;
};

class Cssm : public Object
{
public:
	typedef CssmImpl Impl;
	explicit Cssm(Impl *impl) : Object(impl) {}
	explicit Cssm() : Object(new Impl()) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }

	static Cssm standard() { return CssmImpl::standard(); }
};

} // end namespace CssmClient 

} // end namespace Security

#endif // _H_CDSA_CLIENT_CSSMCLIENT
