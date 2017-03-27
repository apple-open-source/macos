#include <CoreServices/CoreServices.h>
#include <Block.h>
#include <libkern/OSAtomic.h>
#include <syslog.h>
#include "Transform.h"
#include "StreamSource.h"
#include "SingleShotSource.h"
#include "Monitor.h"
#include "Utilities.h"
#include "c++utils.h"
#include "misc.h"
#include "SecTransformInternal.h"
#include "GroupTransform.h"
#include "GroupTransform.h"
#include <pthread.h>

static const int kMaxPendingTransactions = 20;

static CFTypeID internalID = _kCFRuntimeNotATypeID;

// Use &dispatchQueueToTransformKey as a key to dispatch_get_specific to map from
// a transforms master, activation, or any attribute queue to the Transform*
static unsigned char dispatchQueueToTransformKey;

static char RandomChar()
{
	return arc4random() % 26 + 'A'; // good enough
}


static CFStringRef ah_set_describe(const void *v) {
	transform_attribute *ta = ah2ta(static_cast<SecTransformAttributeRef>(const_cast<void*>(v)));
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%@=%@ (conn: %@)"), ta->transform->GetName(), ta->name, ta->value ? ta->value : CFSTR("NULL"), ta->connections ? static_cast<CFTypeRef>(ta->connections) : static_cast<CFTypeRef>(CFSTR("NONE")));
}

static CFStringRef AttributeHandleFormat(CFTypeRef ah, CFDictionaryRef dict) {
	transform_attribute *ta = ah2ta(ah);
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%@"), ta->transform->GetName(), ta->name);
}

static CFStringRef AttributeHandleDebugFormat(CFTypeRef ah) {
	transform_attribute *ta = ah2ta(ah);
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%@ (%p)"), ta->transform->GetName(), ta->name, ta);
}

static void AttributeHandleFinalize(CFTypeRef ah)
{
	transform_attribute *ta = ah2ta(ah);
	if (!ta)
	{
		return;
	}

	if (ta->transform)
	{
		// When we release AH's we clear out the transform pointer, so if we get here with transform!=NULL somebody 
		// has released an AH we are still using and we will crash very very soon (in our code) if we don't abort here
		syslog(LOG_ERR, "over release of SecTransformAttributeRef at %p\n", ah);
		abort();
	}
	
	if (ta->value)
	{
		CFRelease(ta->value);
	}
	
	// ta->q already released
	
	if (ta->connections)
	{
		CFRelease(ta->connections);
	}
	
	if (ta->semaphore)
	{
		dispatch_release(ta->semaphore);
	}
	
	if (ta->attribute_changed_block)
	{
		Block_release(ta->attribute_changed_block);
	}
	
	if (ta->attribute_validate_block)
	{
		Block_release(ta->attribute_validate_block);
	}
	
	free(ta);
}



static CFHashCode ah_set_hash(const void *v) {
	return CFHash(ah2ta(static_cast<SecTransformAttributeRef>(const_cast<void*>(v)))->name);
}

static Boolean ah_set_equal(const void *v1, const void *v2) {
	return CFEqual(ah2ta(static_cast<SecTransformAttributeRef>(const_cast<void*>(v1)))->name, ah2ta(static_cast<SecTransformAttributeRef>(const_cast<void*>(v2)))->name);
}

CFTypeID transform_attribute::cftype;

SecTransformAttributeRef Transform::makeAH(transform_attribute *ta) {
	if (ta) {
		SecTransformAttributeRef ah = _CFRuntimeCreateInstance(NULL, transform_attribute::cftype, sizeof(struct transform_attribute*), NULL);
		if (!ah) {
			return NULL;
		}
		*(struct transform_attribute **)(1 + (CFRuntimeBase*)ah) = ta;
		return ah;
	} else {
		return NULL;
	}
}

static pthread_key_t ah_search_key_slot;

static void destroy_ah_search_key(void *ah) {
	CFRelease(ah);
	pthread_setspecific(ah_search_key_slot, NULL);
}



SecTransformAttributeRef Transform::getAH(SecTransformStringOrAttributeRef attrib, bool create_ok, bool create_underscore_ok)
{
	if (CFGetTypeID(attrib) == transform_attribute::cftype)
	{
		return (SecTransformAttributeRef)attrib;
	}
	
	CFStringRef label = (CFStringRef)attrib;
	static dispatch_once_t once = 0;
	const char *name = (const char *)"SecTransformAttributeRef";
	static CFRuntimeClass ahclass;
	static CFSetCallBacks tasetcb;
	
	dispatch_once(&once, ^{
		ahclass.className = name;
		ahclass.copyFormattingDesc = AttributeHandleFormat;
		ahclass.copyDebugDesc = AttributeHandleDebugFormat;
		ahclass.finalize = AttributeHandleFinalize;
		transform_attribute::cftype = _CFRuntimeRegisterClass(&ahclass);
		if (transform_attribute::cftype == _kCFRuntimeNotATypeID) {
			abort();
		}
		
		tasetcb.equal = ah_set_equal;
		tasetcb.hash = ah_set_hash;
		tasetcb.copyDescription = ah_set_describe;
		
		pthread_key_create(&ah_search_key_slot, destroy_ah_search_key);
	});
	
	SecTransformAttributeRef search_for = pthread_getspecific(ah_search_key_slot);
	if (!search_for)
	{
		search_for = makeAH((transform_attribute*)malloc(sizeof(transform_attribute)));
		if (!search_for)
		{
			return NULL;
		}
		
		bzero(ah2ta(search_for), sizeof(transform_attribute));
		pthread_setspecific(ah_search_key_slot, search_for);
	}
	
	if (!mAttributes)
	{
		mAttributes = CFSetCreateMutable(NULL, 0, &tasetcb);
	}
	
	ah2ta(search_for)->name = label;
	SecTransformAttributeRef ah = static_cast<SecTransformAttributeRef>(const_cast<void*>(CFSetGetValue(mAttributes, search_for)));
	if (ah == NULL && create_ok)
	{
		if (CFStringGetLength(label) && L'_' == CFStringGetCharacterAtIndex(label, 0) && !create_underscore_ok)
		{
			// Attributes with a leading _ belong to the Transform system only, not to random 3rd party transforms.
			return NULL;
		}
		
		transform_attribute *ta = static_cast<transform_attribute *>(malloc(sizeof(transform_attribute)));
		ah = makeAH(ta);
		if (!ah)
		{
			return NULL;
		}
		
		ta->name = CFStringCreateCopy(NULL, label);
		if (!ta->name)
		{
			free(ta);
			return NULL;
		}
		CFIndex cnt = CFSetGetCount(mAttributes);
		CFSetAddValue(mAttributes, ah);
		if (CFSetGetCount(mAttributes) != cnt+1)
		{
			CFRelease(ta->name);
			free(ta);
			return NULL;
		}
		
		CFStringRef qname = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s/%@"), dispatch_queue_get_label(this->mDispatchQueue), label);
		CFIndex used, sz = 1+CFStringGetMaximumSizeForEncoding(CFStringGetLength(qname), kCFStringEncodingUTF8);
		UInt8 *qnbuf = (UInt8 *)alloca(sz);
		CFStringGetBytes(qname, CFRangeMake(0, CFStringGetLength(qname)), kCFStringEncodingUTF8, '?', FALSE, qnbuf, sz, &used);
		qnbuf[used] = '\0';
		ta->q = dispatch_queue_create((char*)qnbuf, NULL);
		CFRelease(qname);
		ta->semaphore = dispatch_semaphore_create(kMaxPendingTransactions);

		
		ta->pushback_state = transform_attribute::pb_empty;
		ta->pushback_value = NULL;
		ta->value = NULL;
		ta->connections = NULL;
		ta->transform = this;

		dispatch_set_target_queue(ta->q, mDispatchQueue);
		ta->required = 0;
		ta->requires_outbound_connection = 0;
		ta->deferred = 0;
		ta->stream = 0;
		ta->ignore_while_externalizing = 0;
		ta->has_incoming_connection = 0;
		ta->direct_error_handling = 0;
		ta->allow_external_sets = 0;
		ta->has_been_deferred = 0;
		ta->attribute_changed_block = NULL;
		ta->attribute_validate_block = NULL;
	}

	return ah;
}

transform_attribute *Transform::getTA(SecTransformStringOrAttributeRef attrib, bool create_ok)
{
	SecTransformAttributeRef ah = getAH(attrib, create_ok);
	if (ah)
	{
		return ah2ta(ah);
	}
	else
	{
		return NULL;
	}
}	



void Transform::TAGetAll(transform_attribute **attributes) {
	CFSetGetValues(mAttributes, (const void**)attributes);
	CFIndex i, n = CFSetGetCount(mAttributes);
	for(i = 0; i < n; ++i) {
		attributes[i] = ah2ta(attributes[i]);
	}
}



bool Transform::HasNoOutboundConnections()
{
	// make an array big enough to hold all of the attributes
	CFIndex numAttributes = CFSetGetCount(mAttributes);
	transform_attribute* attributes[numAttributes];
	
	TAGetAll(attributes);
	
	// check all of the attributes
	CFIndex i;
	for (i = 0; i < numAttributes; ++i)
	{
		if (attributes[i]->connections && CFArrayGetCount(attributes[i]->connections) != 0)
		{
			return false;
		}
	}
	
	return true;
}



bool Transform::HasNoInboundConnections()
{
	// make an array big enough to hold all of the attributes
	CFIndex numAttributes = CFSetGetCount(mAttributes);
	transform_attribute* attributes[numAttributes];
	
	TAGetAll(attributes);
	
	// check all of the attributes
	CFIndex i;
	for (i = 0; i < numAttributes; ++i)
	{
		if (attributes[i]->has_incoming_connection)
		{
			return false;
		}
	}
	
	return true;
}



CFIndex Transform::GetAttributeCount()
{
	return CFSetGetCount(mAttributes);
}

Transform::Transform(CFStringRef transformType, CFStringRef CFobjectType) :
	CoreFoundationObject(CFobjectType), 
	mIsActive(false),
	mIsFinalizing(false),
	mAlwaysSelfNotify(false), 
	mGroup(NULL), 
	mAbortError(NULL),
	mTypeName(CFStringCreateCopy(NULL, transformType))
{
	mAttributes = NULL;
	mPushedback = NULL;
	mProcessingPushbacks = FALSE;
	
	if (internalID == _kCFRuntimeNotATypeID) {
		(void)SecTransformNoData();
		internalID = CoreFoundationObject::FindObjectType(gInternalCFObjectName);
	}
	
	// create a name for the transform
	char rname[10];
	unsigned i;
	for (i = 0; i < sizeof(rname) - 1; ++i)
	{
		rname[i] = RandomChar();
	}
	
	rname[i] = 0;
	
	char *tname = const_cast<char*>(CFStringGetCStringPtr(transformType, kCFStringEncodingUTF8));
	if (!tname) {
		CFIndex sz = 1+CFStringGetMaximumSizeForEncoding(CFStringGetLength(transformType), kCFStringEncodingUTF8);
		tname = static_cast<typeof(tname)>(alloca(sz));
		if (tname) {
			CFStringGetCString(transformType, tname, sz, kCFStringEncodingUTF8);
		} else {
			tname = const_cast<char*>("-");
		}
	}
	
	char* name;
	asprintf(&name, "%s-%s", rname, tname);

	char *dqName;
	asprintf(&dqName, "%s-%s", rname, tname);
	
	char *aqName;
	asprintf(&aqName, "aq-%s-%s", rname, tname);

	mDispatchQueue = dispatch_queue_create(dqName, NULL);
    dispatch_queue_set_specific(mDispatchQueue, &dispatchQueueToTransformKey, this, NULL);
	// mActivationQueue's job in life is to be suspended until just after this transform is made active.
	// It's primary use is when a value flowing across a connection isn't sure if the target transform is active yet.
	mActivationQueue = dispatch_queue_create(aqName, NULL);
	dispatch_set_target_queue(mActivationQueue, mDispatchQueue);
	dispatch_suspend(mActivationQueue);
	mActivationPending = dispatch_group_create();
	
	// set up points for ABORT, DEBUG, INPUT, and OUTPUT
	AbortAH = getAH(kSecTransformAbortAttributeName, true);
	transform_attribute *ta = ah2ta(AbortAH);
	ta->ignore_while_externalizing = 1;
	CFStringRef attributeName = CFStringCreateWithCStringNoCopy(NULL, name, 0, kCFAllocatorMalloc);
	SetAttributeNoCallback(kSecTransformTransformName, attributeName);
	CFRelease(attributeName);
	
	free(dqName);
	free(aqName);

	DebugAH = getAH(kSecTransformDebugAttributeName, true);
	ah2ta(DebugAH)->ignore_while_externalizing = 1;
	
	ta = getTA(kSecTransformInputAttributeName, true);
	ta->required = ta->deferred = ta->stream = 1;
	ta->allow_external_sets = 0;
	ta->value = NULL;
	ta->has_been_deferred = 0;
	ta = getTA(kSecTransformOutputAttributeName, true);
	ta->requires_outbound_connection = ta->stream = 1;
}

static void run_and_release_finalizer(void *finalizer_block)
{
	((dispatch_block_t)finalizer_block)();
	Block_release(finalizer_block);
}

static void set_dispatch_finalizer(dispatch_object_t object, dispatch_block_t finalizer)
{
	finalizer = Block_copy(finalizer);
	dispatch_set_context(object, finalizer);
	dispatch_set_finalizer_f(object, run_and_release_finalizer);
}

void Transform::FinalizePhase2()
{
	delete this;
}

void Transform::FinalizeForClang()
{
	CFIndex numAttributes = CFSetGetCount(mAttributes);
	SecTransformAttributeRef handles[numAttributes];
	CFSetGetValues(mAttributes, (const void**)&handles);
	
	for(CFIndex i = 0; i < numAttributes; ++i) {
		SecTransformAttributeRef ah = handles[i];
		transform_attribute *ta = ah2ta(ah);
		
		set_dispatch_finalizer(ta->q, ^{
			// NOTE: not done until all pending use of the attribute queue has ended AND retain count is zero
			ta->transform = NULL;
			CFRelease(ah);
		});
		// If there is a pending pushback the attribute queue will be suspended, and needs a kick before it can be destructed.
		if (__sync_bool_compare_and_swap(&ta->pushback_state, transform_attribute::pb_value, transform_attribute::pb_discard)) {
			dispatch_resume(ta->q);
		}
		dispatch_release(ta->q);
	}
	
	// We might be finalizing a transform as it is being activated, make sure that is complete before we do the rest
    dispatch_group_notify(mActivationPending, mDispatchQueue, ^{
        if (mActivationQueue != NULL) {
            // This transform has not been activated (and does not have a activation pending), so we need to resume to activation queue before we can release it
            dispatch_resume(mActivationQueue);
            dispatch_release(mActivationQueue);
        }
        
        set_dispatch_finalizer(mDispatchQueue, ^{
            // NOTE: delayed until all pending work items on the transform's queue are complete, and all of the attribute queues have been finalized, and the retain count is zero
            FinalizePhase2();
        });
        dispatch_release(mDispatchQueue);
    });
}

void Transform::Finalize()
{
	// When _all_ transforms in the group have been marked as finalizing we can tear down our own context without anyone else in the group sending us values
	// (NOTE: moved block into member function as clang hits an internal error and declines to compile)
	dispatch_block_t continue_finalization = ^{ this->FinalizeForClang(); };
	dispatch_block_t mark_as_finalizing = ^{ this->mIsFinalizing = true; };
    
	// Mark the transform as "finalizing" so it knows not to propagate values across connections
    if (this == dispatch_get_specific(&dispatchQueueToTransformKey)) {
        mark_as_finalizing();
    } else {
        dispatch_sync(mDispatchQueue, mark_as_finalizing);
    }
	
	if (mGroup) {
        (void)transforms_assume(mGroup->mIsFinalizing);  // under retain?
        mGroup->AddAllChildrenFinalizedCallback(mDispatchQueue, continue_finalization);
        mGroup->ChildStartedFinalization(this);
	} else {
		// a "bare" transform (normally itself a group) still needs to be deconstructed
		dispatch_async(mDispatchQueue, continue_finalization);
	}			
}

Transform::~Transform()
{
	CFRelease(mAttributes);
	if (mAbortError) {
		CFRelease(mAbortError);
        mAbortError = NULL;
	}
	
	// See if we can catch anything using us after our death
	mDispatchQueue = (dispatch_queue_t)0xdeadbeef;
	
	CFRelease(mTypeName);
	
	if (NULL != mPushedback)
	{
		CFRelease(mPushedback);
	}
	dispatch_release(mActivationPending);
}

CFStringRef Transform::GetName() {
	return (CFStringRef)GetAttribute(kSecTransformTransformName);
}

CFTypeID Transform::GetCFTypeID()
{
	return CoreFoundationObject::FindObjectType(CFSTR("SecTransform"));
}

std::string Transform::DebugDescription()
{
	return CoreFoundationObject::DebugDescription() + "|SecTransform|" + StringFromCFString(this->GetName());
}

CFErrorRef Transform::SendMetaAttribute(SecTransformStringOrAttributeRef key, SecTransformMetaAttributeType type, CFTypeRef value)
{
	SecTransformAttributeRef ah = getAH(key, true);
	transform_attribute *ta = ah2ta(ah);
	switch (type)
	{
		case kSecTransformMetaAttributeRequired:
			ta->required = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
			break;
			
		case kSecTransformMetaAttributeRequiresOutboundConnection:
			ta->requires_outbound_connection = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
			break;
			
		case kSecTransformMetaAttributeDeferred:
			ta->deferred = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
			break;
			
		case kSecTransformMetaAttributeStream:
			ta->stream = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
			break;
			
		case kSecTransformMetaAttributeHasOutboundConnections:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't set kSecTransformMetaAttributeHasOutboundConnections for %@ (or any other attribute)", ah);
			
		case kSecTransformMetaAttributeHasInboundConnection:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't set kSecTransformMetaAttributeHasInboundConnection for %@ (or any other attribute)", ah);
			
		case kSecTransformMetaAttributeCanCycle:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "kSecTransformMetaAttributeCanCycle not yet supported (%@)", ah);
			
		case kSecTransformMetaAttributeExternalize:
			ta->ignore_while_externalizing = CFBooleanGetValue((CFBooleanRef)value) ? 0 : 1;
			break;
			
		case kSecTransformMetaAttributeValue:
			return SetAttributeNoCallback(ah, value);
			
		case kSecTransformMetaAttributeRef:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't set kSecTransformMetaAttributeRef for %@ (or any other attribute)", ah);
			
		case kSecTransformMetaAttributeName:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't set kSecTransformMetaAttributeName for %@ (or any other attribute)", ah);
			
		default:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't set unknown meta attribute #%d to %@ on %@", type, value, key);
	}
	
	return NULL;
}

CFTypeRef Transform::GetMetaAttribute(SecTransformStringOrAttributeRef key, SecTransformMetaAttributeType type) {
	SecTransformAttributeRef ah = getAH(key, true);
	transform_attribute *ta = ah2ta(ah);
	switch (type) {
		case kSecTransformMetaAttributeRequired:
			return (CFTypeRef)(ta->required ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeRequiresOutboundConnection:
			return (CFTypeRef)(ta->requires_outbound_connection ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeDeferred:
			return (CFTypeRef)(ta->deferred ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeStream:
			return (CFTypeRef)(ta->stream ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeHasOutboundConnections:
			return (CFTypeRef)((ta->connections && CFArrayGetCount(ta->connections)) ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeHasInboundConnection:
			return (CFTypeRef)(ta->has_incoming_connection ? kCFBooleanTrue : kCFBooleanFalse);
		case kSecTransformMetaAttributeCanCycle:
			return (CFTypeRef)kCFBooleanFalse;
		case kSecTransformMetaAttributeExternalize:
			return (CFTypeRef)(ta->ignore_while_externalizing ? kCFBooleanFalse : kCFBooleanTrue);
		case kSecTransformMetaAttributeRef:
			return ah;
		case kSecTransformMetaAttributeValue:
			return ta->value;
		case kSecTransformMetaAttributeName:
			return ta->name;
		default:
			return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Can't get unknown meta attribute #%d from %@", type, key);
	}
	
	return NULL;
}



CFErrorRef Transform::RefactorErrorToIncludeAbortingTransform(CFErrorRef sourceError)
{
	// pull apart the error
	CFIndex code = CFErrorGetCode(sourceError);
	CFStringRef domain = CFErrorGetDomain(sourceError);
	CFDictionaryRef oldUserInfo = CFErrorCopyUserInfo(sourceError);
	CFMutableDictionaryRef userInfo = CFDictionaryCreateMutableCopy(NULL, 0, oldUserInfo);
	CFRelease(oldUserInfo);
	
	// add the new key and value to the dictionary
	CFDictionaryAddValue(userInfo, kSecTransformAbortOriginatorKey, GetCFObject());
	
	// make a new CFError
	CFErrorRef newError = CFErrorCreate(NULL, domain, code, userInfo);
	CFRelease(userInfo);
	return newError;
}

// NOTE: If called prior to execution will schedule a later call to AbortAllTransforms
void Transform::AbortJustThisTransform(CFErrorRef abortErr)
{
    (void)transforms_assume(abortErr);
    (void)transforms_assume(dispatch_get_specific(&dispatchQueueToTransformKey) == this);
    
    Boolean wasActive = mIsActive;

    if (OSAtomicCompareAndSwapPtr(NULL, (void *)abortErr, (void**)&mAbortError)) {
		// send an abort message to the attribute so that it can shut down
		// note that this bypasses the normal processes.  The message sent is a notification
		// that things aren't working well any more, the transform cannot make any other assumption.

        // mAbortError is released in the destructor which is triggered (in part)
        // by the dispatch queue finalizer so we don't need a retain/release of
        // abortErr for the abortAction block, but we do need to retain it
        // here to match with the release by the destructor.
        CFRetain(abortErr);
        
		dispatch_block_t abortAction = ^{
            // This actually makes the abort happen, it needs to run on the transform's queue while the
            // transform is executing.
            
            if (!wasActive) {
                // When this abort was first processed we were not executing, so
                // additional transforms may have been added to our group (indeed,
                // we may not have had a group at all), so we need to let everyone
                // know about the problem.   This will end up letting ourself (and
                // maybe some others) know an additional time, but the CompareAndSwap
                // prevents that from being an issue.
                this->AbortAllTransforms(abortErr);
            }
            
            SecTransformAttributeRef GCC_BUG_WORKAROUND inputAttributeHandle = getAH(kSecTransformInputAttributeName, false);
            // Calling AttributeChanged directly lets an error "skip ahead" of the input queue,
            // and even execute if the input queue is suspended pending pushback retries.
            AttributeChanged(inputAttributeHandle, abortErr);
            try_pushbacks();
        };
        
        if (mIsActive) {
            // This transform is running, so we use the normal queue (which we are
            // already executing on)
            abortAction();
        } else {
            // This transform hasn't run yet, do the work on the activation queue
            // so it happens as soon as the transforms starts executing.
            dispatch_async(mActivationQueue, abortAction);
        }
	} else {
        Debug("%@ set to %@ while processing ABORT=%@, this extra set will be ignored", AbortAH, abortErr, mAbortError);
    }
}

// abort all transforms in the root group & below
void Transform::AbortAllTransforms(CFTypeRef err)
{
	Debug("%@ set to %@, aborting\n", AbortAH, err);
	CFErrorRef error = NULL;
	
	CFTypeRef replacementErr = NULL;
	
	if (CFGetTypeID(err) != CFErrorGetTypeID())
	{
		replacementErr = err = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "ABORT set to a %@ (%@) not a %@", CFCopyTypeIDDescription(CFGetTypeID(err)), err, CFCopyTypeIDDescription(CFErrorGetTypeID()));
	}
	
	error = RefactorErrorToIncludeAbortingTransform((CFErrorRef)err);

	if (replacementErr)
	{
		CFRelease(replacementErr);
	}
	
    GroupTransform *root = GetRootGroup();
	if (root)
	{
		// tell everyone in the (root) group to "AbortJustThisTransform"
        dispatch_group_t all_aborted = dispatch_group_create();
        root->ForAllNodesAsync(false, all_aborted, ^(Transform* t){
            t->AbortJustThisTransform(error);
        });
        dispatch_group_notify(all_aborted, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(void) {
            CFRelease(error);
            dispatch_release(all_aborted);
        });
	}
	else
	{
		// We are everyone so we AbortJustThisTransform "directly"
        // NOTE: this can only happen prior to execution (execution always happens in a group)
        (void)transforms_assume_zero(mIsActive);
		this->AbortJustThisTransform(error);
	}
}



CFErrorRef Transform::Disconnect(Transform* destinationTransform, CFStringRef myKey, CFStringRef hisKey)
{
	//CFTypeRef thisTransform = (SecTransformRef) GetCFObject();
	
	// find this transform in the backlinks for the destination
	CFIndex i;

	// now remove the link in the transform dictionary
	transform_attribute *src = getTA(myKey, true);
	SecTransformAttributeRef dst = destinationTransform->getAH(hisKey);
	
	if (src->connections == NULL)
	{
		return CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "Cannot find transform in destination.");
	}
	
	CFIndex numConnections = CFArrayGetCount(src->connections);
	for (i = 0; i < numConnections; ++i)
	{
		if (CFArrayGetValueAtIndex(src->connections, i) == dst)
		{
			CFArrayRemoveValueAtIndex(src->connections, i);
			numConnections = CFArrayGetCount(src->connections);
		}
		
		// clear the has_incoming_connection bit in the destination.  We can do this because inputs can have only one connection.
		transform_attribute* dstTA = ah2ta(dst);
		dstTA->has_incoming_connection = false;
	}
	
	if (HasNoInboundConnections() && HasNoOutboundConnections())
	{
		// we have been orphaned, just remove us
		mGroup->RemoveMemberFromGroup(GetCFObject());
		mGroup = NULL;
	}
	
	return NULL;
}



CFErrorRef Transform::Connect(GroupTransform *group, Transform* destinationTransform, CFStringRef destAttr, CFStringRef srcAttr)
{
	if (group == NULL) 
	{
		CFErrorRef err = CreateSecTransformErrorRef(kSecTransformErrorInvalidConnection, "Can not make connections without a specific group (do not call with group = NULL)");
		return err;
	}
    
    GroupTransform *newSourceGroup = mGroup;
    GroupTransform *newDestinationGroup = destinationTransform->mGroup;
	
	if (mGroup == NULL || mGroup == this) 
	{
		newSourceGroup = group;
	}
	
	if (destinationTransform->mGroup == NULL || destinationTransform->mGroup == destinationTransform) 
	{
		newDestinationGroup = group;
	}
	
	if (newSourceGroup != newDestinationGroup && mGroup) 
	{
        CFErrorRef err = CreateSecTransformErrorRef(kSecTransformErrorInvalidConnection, "Can not make connections between transforms in different groups (%@ is in %@, %@ is in %@)", GetName(), newSourceGroup->GetName(), destinationTransform->GetName(), newDestinationGroup->GetName());
		return err;
	}
    
    if (!validConnectionPoint(srcAttr)) {
        CFErrorRef err = CreateSecTransformErrorRef(kSecTransformErrorInvalidConnection, "Can not make a connection from non-exported attribute %@ of %@", srcAttr, this->GetName());
        return err;
    }
    if (!destinationTransform->validConnectionPoint(destAttr)) {
        CFErrorRef err = CreateSecTransformErrorRef(kSecTransformErrorInvalidConnection, "Can not make a connection to non-exported attribute %@ of %@", destAttr, destinationTransform->GetName());
        return err;
    }
    
    mGroup = newSourceGroup;
    destinationTransform->mGroup = newDestinationGroup;
		
	// NOTE: this fails on OOM
	group->AddMemberToGroup(this->GetCFObject());
	group->AddMemberToGroup(destinationTransform->GetCFObject());
	
	transform_attribute *src = this->getTA(srcAttr, true);
	SecTransformAttributeRef dst = destinationTransform->getAH(destAttr);
	
	if (!src->connections) 
	{
		src->connections = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(src->connections, dst);
	
	ah2ta(dst)->has_incoming_connection = 1;
	
	return NULL;
}


bool Transform::validConnectionPoint(CFStringRef attributeName)
{
    return true;
}

// NoCallback == don't call this transform's Do function, but DO call the Do functions of connected attributes
// SetAttribute eventually calls SetAttributeNoCallback
CFErrorRef Transform::SetAttributeNoCallback(SecTransformStringOrAttributeRef key, CFTypeRef value)
{
	SecTransformAttributeRef ah = getAH(key, true);
	if (!ah) 
	{
		abort();
	}
	transform_attribute *ta = ah2ta(ah);
	
	if (ah == AbortAH && value && (mIsActive || !ta->deferred))
	{
		AbortAllTransforms(value);
		return CreateSecTransformErrorRef(kSecTransformErrorAbortInProgress, "Abort started");
	}
	
	bool do_propagate = true;
	
	if (!ta->has_been_deferred)
	{
		bool doNotRetain = false;

		if (value) 
		{
			CFStringRef name = ta->name;
			if (CFGetTypeID(value) == CFReadStreamGetTypeID()) 
			{
				CFTypeRef src = StreamSource::Make((CFReadStreamRef) value, this, name);
				value = src;
				do_propagate = false;
				ta->has_been_deferred = 1;
				doNotRetain = true;
			}
			else if (ta->deferred && !mIsActive)
			{
				if (ta->deferred)
				{
					Debug("%@ deferred value=%p\n", ah, value);
				}
				
				CFTypeRef src = SingleShotSource::Make(value, this, name);
				ta->has_been_deferred = 1;
				
				// the old value will be release when Transform::Do terminates
				
				value = src;
				do_propagate = false;
				doNotRetain = true;
			}
			else
			{
				ta->has_been_deferred = 0;
			}
		}
		
		if (ta->value != value) {
			if (value && !doNotRetain) {
				CFRetain(value);
			}
			if (ta->value) {
				CFRelease(ta->value);
			}
		}
		
		ta->value = value;
	}
	
	// propagate the changes out to all connections
	if (ta->connections && mIsActive && do_propagate && !(mAbortError || mIsFinalizing))
	{
		Debug("Propagating from %@ to %@\n", ah, ta->connections);
		CFIndex i, numConnections = CFArrayGetCount(ta->connections);
		for(i = 0; i < numConnections; ++i) {
			SecTransformAttributeRef ah = static_cast<SecTransformAttributeRef>(const_cast<void *>(CFArrayGetValueAtIndex(ta->connections, i)));
			Transform *tt = ah2ta(ah)->transform;
			if (NULL != tt)
			{
				if (tt->mIsActive)
				{
					tt->SetAttribute(ah, value);
				}
				else
				{
					dispatch_block_t setAttribute = ^{
                        tt->SetAttribute(ah, value);
                    };
                    // Here the target queue might not be activated yet, we can't
                    // look directly at the other transform's ActivationQueue as
                    // it might activate (or Finalize!) as we look, so just ask
                    // the other transform to deal with it.
                    dispatch_async(ah2ta(ah)->q, ^(void) {
                        // This time we are on the right queue to know this is the real deal
                        if (tt->mIsActive) {
                            setAttribute();
                        } else {
                            dispatch_async(ah2ta(ah)->transform->mActivationQueue, setAttribute);
                        }
                    });
				}
			}
		}
	}

	return NULL;
}

// external sets normally fail if the transform is running
CFErrorRef Transform::ExternalSetAttribute(CFTypeRef key, CFTypeRef value)
{
	if (!mIsActive)
	{
		return this->SetAttribute(key, value);
	}
	else
	{
		SecTransformAttributeRef ah = getAH(key, false);
		if (ah != NULL && ah2ta(ah)->allow_external_sets)
		{
			return this->SetAttribute(static_cast<CFTypeRef>(ah), value);
		}
		else
		{
			return CreateSecTransformErrorRef(kSecTransformTransformIsExecuting, "%@ can not be set while %@ is executing", ah, this->GetName());
		}
	}
}


// queue up the setting of the key and value
CFErrorRef Transform::SetAttribute(CFTypeRef key, CFTypeRef value)
{
	if (mAbortError)
	{
		return CreateSecTransformErrorRef(kSecTransformErrorAborted, "ABORT has been sent to the transform (%@)", mAbortError);
	}
	
	// queue up the setting of the key and value
	SecTransformAttributeRef ah;
	if (CFGetTypeID(key) == transform_attribute::cftype) 
	{
		ah = key;
	}
	else if (CFGetTypeID(key) == CFStringGetTypeID())
	{
		ah = getAH(static_cast<CFStringRef>(key));
		if (!ah)
		{
			return CreateSecTransformErrorRef(kSecTransformErrorUnsupportedAttribute, "Can't set attribute %@ in transform %@", key, GetName());
		}
	}
	else
	{
		return CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "Transform::SetAttribute called with %@, requires a string or an AttributeHandle", key);
	}
	
	// Do this after the error check above so we don't leak
	if (value != NULL)
	{
		CFRetain(value); // if we use dispatch_async we need to own the value (the matching release is in the set block)
	}

	
	transform_attribute *ta = ah2ta(ah);

	dispatch_block_t set = ^{
		Do(ah, value);

		dispatch_semaphore_signal(ta->semaphore);

		if (value != NULL)
		{
			CFRelease(value);
		}
	};
	
	
	// when the transform is active, set attributes asynchronously.  Otherwise, we are doing
	// initialization and must wait for the operation to complete.
	if (mIsActive)
	{
		dispatch_async(ta->q, set);
	}
	else
	{
		dispatch_sync(ta->q, set);
	}
	if (dispatch_semaphore_wait(ta->semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC))) {
		Debug("Send from %@ to %@ is still waiting\n", GetName(), ah);
		dispatch_semaphore_wait(ta->semaphore, DISPATCH_TIME_FOREVER);
	}
	
	// Return the best available status (which will be NULL if we haven't aborted, or stated an
	// intent to abort when execution starts)
	//
	// The value of the ABORT attribute can differ from mAbortError, first if a transform is aborted
	// prior to running the general abort mechanic is deferred until execution.   Second during
	// execution the abort logic avoids most of the normal processing.   Third, and most importantly
	// during an abort the exact error that gets generated will differ from the value sent to ABORT
	// (for example if a non-CFError was sent...plus even if it was a CFError we annotate that error).

	return mAbortError;
}

CFErrorRef Transform::SendAttribute(SecTransformStringOrAttributeRef key, CFTypeRef value)
{
	return SetAttributeNoCallback(key, value);
}



CFTypeRef Transform::GetAttribute(SecTransformStringOrAttributeRef key)
{
	struct transform_attribute *ta = getTA(key, false);
	if (ta == NULL || ta->value == NULL) {
		return NULL;
	}
	
	if (CFGetTypeID(ta->value) == internalID)
	{
		// this is one of our internal objects, so get the value from it
		Source* source = (Source*) CoreFoundationHolder::ObjectFromCFType(ta->value);
		return source->GetValue();
	}
	else
	{
		return ta->value;
	}
}

CFErrorRef Transform::Pushback(SecTransformAttributeRef ah, CFTypeRef value) 
{
	CFErrorRef result = NULL;
	transform_attribute *ta = ah2ta(ah);
	if (!(ta->pushback_state == transform_attribute::pb_empty || ta->pushback_state == transform_attribute::pb_repush))
	{
		CFErrorRef error = fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidOperation, CFSTR("Can not pushback new value until old value has been processed"));
		SetAttribute(kSecTransformAbortAttributeName, error);
		return error;
	}
	if (value == NULL && ta->pushback_value == NULL && ta->pushback_state == transform_attribute::pb_repush) 
	{
		ta->pushback_state = transform_attribute::pb_presented_once;
	} else 
	{
		ta->pushback_state = transform_attribute::pb_value;
	}
	if (value) 
	{
		CFRetain(value);
	}
	ta->pushback_value = value;
	dispatch_suspend(ta->q);
	if (!mPushedback) 
	{
		mPushedback = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(mPushedback, ah);
	return result;
}

void Transform::try_pushbacks() {
	if (!mPushedback || !CFArrayGetCount(mPushedback)) {
		mProcessingPushbacks = FALSE;
		return;
	}
	
	CFArrayRef pb = (CFArrayRef)mPushedback;
	mPushedback = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFIndex i, n = CFArrayGetCount(pb);
	int succeeded = 0;
	for(i = 0; i < n; ++i) 
	{
		SecTransformAttributeRef ah = CFArrayGetValueAtIndex(pb, i);
		transform_attribute *ta = ah2ta(ah);
		ta->pushback_state = transform_attribute::pb_repush;
		CFTypeRef v = ta->pushback_value;
		ta->pushback_value = NULL;
		Do(ah, v);
		if (v) 
		{
			CFRelease(v);
		}
		if (ta->pushback_state == transform_attribute::pb_repush) {
			ta->pushback_state = transform_attribute::pb_empty;
			succeeded++;
		}
		// NOTE: a successful repush needs the queue unsuspended so it can run.
		// A failed repush has suspended the queue an additional time, so we
		// still need to resume it.
		dispatch_resume(ta->q);
	}
	
	CFRelease(pb);
	
	if (succeeded && CFArrayGetCount(mPushedback)) {
		// some attribute changed while we proceeded the last batch of pushbacks, so any "new" pushbacks are eligible to run again.
		// In theory the ones that were pushed after the last success don't need to be re-run but that isn't a big deal.
		dispatch_async(mDispatchQueue, ^{ try_pushbacks(); });
	} else {
		mProcessingPushbacks = FALSE;
	}
}

void Transform::Debug(const char *cfmt, ...) {
	CFTypeRef d = ah2ta(DebugAH)->value;
	if (d) {
		CFWriteStreamRef out = NULL;
		if (CFGetTypeID(d) == CFWriteStreamGetTypeID()) {
			out = (CFWriteStreamRef)d;
		} else {
			static dispatch_once_t once;
			static CFWriteStreamRef StdErrWriteStream;
			dispatch_once(&once, ^{
				CFURLRef p = CFURLCreateWithFileSystemPath(NULL, CFSTR("/dev/stderr"), kCFURLPOSIXPathStyle, FALSE);
				StdErrWriteStream = CFWriteStreamCreateWithFile(NULL, p);
				CFWriteStreamOpen(StdErrWriteStream);
				CFRelease(p);
			});
			out = StdErrWriteStream;
		}
		
		va_list ap;
		va_start(ap, cfmt);
		
		CFStringRef fmt = CFStringCreateWithCString(NULL, cfmt, kCFStringEncodingUTF8);
		CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, ap);
		CFRelease(fmt);
		va_end(ap);

		
		CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		sz += 1;
		CFIndex used = 0;
		unsigned char *buf;
		bool needs_free = true;
		buf = (unsigned char*)malloc(sz);
		if (buf) {
			CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, '?', FALSE, buf, sz, &used);
		} else {
			buf = (unsigned char *)"malloc failure during Transform::Debug\n";
			needs_free = false;
		}
		
		static dispatch_once_t once;
		static dispatch_queue_t print_q;
		dispatch_once(&once, ^{
			print_q = dispatch_queue_create("com.apple.security.debug.print_queue", 0);
			dispatch_set_target_queue((dispatch_object_t)print_q, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0));
		});
		
		dispatch_async(print_q, ^{
			CFWriteStreamWrite(out, buf, used);
			if (needs_free) {
				free(buf);
			}
		});
		
		CFRelease(str);
	}
}

void Transform::Do(SecTransformAttributeRef ah, CFTypeRef value)
{
	transform_attribute *ta = ah2ta(ah);
	if (ta->pushback_state == transform_attribute::pb_discard)
	{
		return;
	}
	(void)transforms_assume(dispatch_get_current_queue() == ((ta->pushback_state == transform_attribute::pb_repush) ? mDispatchQueue : ta->q));
	
	if (mIsFinalizing)
	{
		Debug("Ignoring value %p sent to %@ (on queue %s) during finalization", value, ah, dispatch_queue_get_label(dispatch_get_current_queue()));
		return;
	}
	
	SetAttributeNoCallback(ah, value);
    // While an abort is in progress things can get into bad
    // states if we allow normal processing so we throw anything
    // on the floor except CFErrorRef or NULL vales sent to
    // ABORT or INPUT (we need to process them to let the
    // transform shut down correctly)
	if (mAbortError && (!(ah == this->AbortAH || ah == getTA(CFSTR("INPUT"), true)) && (value == NULL || CFGetTypeID(value) != CFErrorGetTypeID())))
	{
		if (value) {
            Debug("Ignoring value (%@) sent to %@ during abort\n", value, ah);            
        } else {
            Debug("Ignoring NULL sent to %@ during abort\n", ah);            
        }
		return;
	}
	
	if (mIsActive || (mAlwaysSelfNotify && !ta->deferred)) 
	{
		Debug("AttributeChanged: %@ (%s) = %@\n", ah, mIsActive ? "is executing" : "self notify set", value ? value : (CFTypeRef)CFSTR("(NULL)"));
		AttributeChanged(ah, value);
	} 
	
	if (mPushedback && CFArrayGetCount(mPushedback) && !mProcessingPushbacks) 
	{
		Debug("will process pushbacks (%@) later\n", mPushedback);
		mProcessingPushbacks = TRUE;
		dispatch_async(mDispatchQueue, ^{ try_pushbacks(); });
	}
	
	return;
}


void Transform::AttributeChanged(CFStringRef name, CFTypeRef value)
{
}

void Transform::AttributeChanged(SecTransformAttributeRef ah, CFTypeRef value)
{
	AttributeChanged(ah2ta(ah)->name, value);
}

CFArrayRef Transform::GetAllAH() {
	CFIndex cnt = CFSetGetCount(mAttributes);
	const void **values = (const void **)alloca(sizeof(void*)*cnt);
	CFSetGetValues(mAttributes, values);
	return CFArrayCreate(NULL, values, cnt, &kCFTypeArrayCallBacks);
}

CFTypeRef Transform::Execute(dispatch_queue_t deliveryQueue, SecMessageBlock deliveryBlock, CFErrorRef* errorRef)
{
	if (!mGroup)
	{
		CFTypeRef g = GroupTransform::Make();
		mGroup = (GroupTransform*)CoreFoundationHolder::ObjectFromCFType(g);
		mGroup->AddMemberToGroup(this->GetCFObject());
		SecMessageBlock smb = ^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
		{
			deliveryBlock(message, error, isFinal);
			if (isFinal)
			{
				dispatch_async(this->mDispatchQueue, ^{
					CFRelease(g);
				});
			}
		};
		
		CFTypeRef ret = this->Execute(deliveryQueue, deliveryBlock ? smb : (SecMessageBlock) NULL, errorRef);
		
		if (!deliveryBlock)
		{
			CFRelease(g);
		}
		
		return ret;
	}

	if (mIsActive)
	{
		if (errorRef)
		{
			*errorRef = CreateSecTransformErrorRef(kSecTransformTransformIsExecuting, "The %@ transform has already executed, it may not be executed again.", GetName());
		}
		
		return NULL;
	}

	// Do a retain on our parent since we are using it
    GroupTransform *rootGroup = GetRootGroup();
	CFRetain(rootGroup->GetCFObject());

	CFTypeRef result = NULL;
	
	CFTypeRef monitorRef =  BlockMonitor::Make(deliveryQueue, deliveryBlock);
	
	__block CFStringRef outputAttached = NULL;
	
	dispatch_queue_t p2 = dispatch_queue_create("activate phase2", NULL);
	dispatch_queue_t p3 = dispatch_queue_create("activate phase3", NULL);
	dispatch_suspend(p2);
	dispatch_suspend(p3);
	// walk the transform, doing phase1 activating as we go, and queueing phase2 and phase3 work
	CFErrorRef temp = TraverseTransform(NULL, ^(Transform *t){
        return t->ExecuteOperation(outputAttached, (SecMonitorRef)monitorRef, p2, p3);
	});
	// ExecuteOperation is not called for the outer group, so we need to manually set mISActive for it.
	rootGroup->mIsActive = true;
    rootGroup->StartingExecutionInGroup();
	dispatch_resume(p2);
	dispatch_sync(p2, ^{ dispatch_resume(p3); });
	dispatch_sync(p3, ^{ dispatch_release(p2); });
	dispatch_release(p3);
	
	if (errorRef)
	{
		*errorRef = temp;
	}
	if (temp) {
        // It is safe to keep the monitors attached, because it is invalid to try to execute again, BUT
        // we do need to release the reference to the group that the monitor would normally release
        // when it processes the final message.
        CFRelease(rootGroup->GetCFObject());
        CFRelease(monitorRef);
        rootGroup->StartedExecutionInGroup(false);
        return NULL;
	}
	
	dispatch_group_t initialized = dispatch_group_create();
	rootGroup->ForAllNodesAsync(true, initialized, ^(Transform*t) {
        t->Initialize();
	});
	
	dispatch_group_notify(initialized, rootGroup->mDispatchQueue, ^{
		dispatch_release(initialized);
		dispatch_group_t activated = dispatch_group_create();
		dispatch_group_enter(activated);
		dispatch_async(rootGroup->mDispatchQueue, ^{
			rootGroup->ForAllNodesAsync(true, activated, ^(Transform*t) {
				t->ActivateInputs();
			});
			dispatch_group_leave(activated);
		});
		dispatch_group_notify(activated, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			dispatch_release(activated);
			// once we have been activated (but not before!), the monitor belongs to the group, and we can drop our claim
			CFRelease(monitorRef);
			rootGroup->StartedExecutionInGroup(true);
		});
	});
	
	return result;
}


void Transform::Initialize()
{
}

static void ActivateInputs_set(const void *v, void *unused) {
	transform_attribute *ta = static_cast<transform_attribute *>(ah2ta(const_cast<void *>(v)));
	if (ta->value && internalID == CFGetTypeID(ta->value)) {
		Source* s = (Source*) CoreFoundationHolder::ObjectFromCFType(ta->value);
		s->Activate();
	}
}

void Transform::ActivateInputs()
{
	(void)transforms_assume_zero(mIsActive && this != dispatch_get_specific(&dispatchQueueToTransformKey));
	
	// now run all of the forward links
	if (!mIsFinalizing) {
		CFSetApplyFunction(mAttributes, ActivateInputs_set, NULL);
	}
}

CFErrorRef Transform::ForAllNodes(bool parallel, bool includeOwningGroup, Transform::TransformOperation op)
{
    GroupTransform *g = GetRootGroup();
	if (g) {
		return g->ForAllNodes(parallel, includeOwningGroup, op);
	} else {
		return op(this);
	}
}

CFErrorRef Transform::TraverseTransform(CFMutableSetRef visited, TransformOperation t)
{
	return ForAllNodes(true, true, t);
}

CFErrorRef Transform::ExecuteOperation(CFStringRef &outputAttached, SecMonitorRef output, dispatch_queue_t phase2, dispatch_queue_t phase3)
{
	if (!mGroup) {
        // top level groups are special, and don't go through this path.
        return NULL;
    }
    
    if (!TransformCanExecute())
	{
		// oops, this transform isn't ready to go
		return CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "The transform %@ was not ready for execution.", GetName());
	}
	
	// check to see if required attributes are connected or set
	CFIndex i, numAttributes = CFSetGetCount(mAttributes);
	transform_attribute **attributes = (transform_attribute **)alloca(numAttributes * sizeof(transform_attribute *));
	TAGetAll(attributes);
	CFMutableArrayRef still_need = NULL;
	for(i = 0; i < numAttributes; ++i) {
		transform_attribute *ta = attributes[i];
		if (ta->required && ta->value == NULL && !ta->has_incoming_connection) {
			if (!still_need) {
				still_need = CFArrayCreateMutable(NULL, i, &kCFTypeArrayCallBacks);
			}
			CFArrayAppendValue(still_need, ta->name);
		}
	}
	if (still_need) {
		CFStringRef elist = CFStringCreateByCombiningStrings(NULL, still_need, CFSTR(", "));
		CFErrorRef err = CreateSecTransformErrorRef(kSecTransformErrorMissingParameter, "Can not execute %@, missing required attributes: %@", GetName(), elist);
		CFRelease(elist);
		CFRelease(still_need);
		return err;
	}		

	// see if we can attach our output here (note mAttributes may have changed)
	numAttributes = CFSetGetCount(mAttributes);
	attributes = (transform_attribute **)alloca(numAttributes * sizeof(transform_attribute *));
	TAGetAll(attributes);	
	for (i = 0; i < numAttributes; ++i)
	{
		transform_attribute *ta = attributes[i];
		CFIndex arraySize = ta->connections ? CFArrayGetCount(ta->connections) : 0;
		if (arraySize == 0 && ta->requires_outbound_connection)
		{
			if (CFStringCompare(ta->name, kSecTransformOutputAttributeName, 0) == kCFCompareEqualTo) {
				// this is a place where we can hook up our output -- maybe
				if (outputAttached)
				{
					// oops, we've already done that.
					return CreateSecTransformErrorRef(kSecTransformErrorMoreThanOneOutput, "Both %@ and %@ have loose outputs, attach one to something", outputAttached, ta->transform->GetName());
				}
				// Delay the connect until after ForAllNodes returns
				dispatch_async(phase2, ^{
					SecTransformConnectTransformsInternal(mGroup->GetCFObject(),
														  GetCFObject(), kSecTransformOutputAttributeName,
														  output, kSecTransformInputAttributeName);
				});
				outputAttached = ta->transform->GetName();
				
				// activate the attached monitor
				Monitor* m = (Monitor*) CoreFoundationHolder::ObjectFromCFType(output);
				m->mIsActive = true;
				
				// add the monitor to the output so that it doesn't get activated twice
			} else {
				return CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "Attribute %@ (in %@) requires an outbound connection and doesn't have one", ta->name, GetName());
			}
			
			break;
		}
	}
	
	// Delay activation until after the Monitor is connected
	dispatch_async(phase3, ^{
		phase3Activation();
	});
	
	return NULL;
}



void Transform::DoPhase3Activation()
{
    this->mIsActive = true;
    // execution has now truly started ("mIsActive is true")
    CFErrorRef initError = TransformStartingExecution();
    if (initError)
    {
        // Oops, now execution is about to grind to a halt
        this->SendAttribute(AbortAH, initError);
    }

    dispatch_resume(this->mActivationQueue);
    dispatch_group_async(this->mActivationPending, this->mActivationQueue, ^{
        dispatch_release(this->mActivationQueue);
        this->mActivationQueue = NULL;
    });
}



// This would be best expressed as a block, but we seem to run into compiler errors
void Transform::phase3Activation()
{
    dispatch_async(this->mDispatchQueue, ^
    {
        DoPhase3Activation();
    });
}


Boolean Transform::TransformCanExecute()
{
	return true;
}



CFErrorRef Transform::TransformStartingExecution()
{
	return NULL;
}



bool Transform::IsExternalizable()
{
	return true;
}

static const void *CFTypeOrNULLRetain(CFAllocatorRef allocator, const void *value) {
	if (value != NULL) {
		return CFRetain(value);
	} else {
		return value;
	}
}

static void CFTypeOrNULLRelease(CFAllocatorRef allocator, const void *value) {
	if (value != NULL) {
		CFRelease(value);
	}
}

static CFStringRef CFTypeOrNULLCopyDescription (const void *value) {
	if (value != NULL) {
		return CFCopyDescription(value);
	} else {
		return CFSTR("NULL");
	}
}

static Boolean CFTypeOrNULLEqual(const void *value1, const void *value2) {
	if (value1 == NULL && value2 == NULL) {
		return TRUE;
	} else {
		if (value1 == NULL || value2 == NULL) {
			return FALSE;
		} else {
			return CFEqual(value1, value2);
		}
	}
}

// Returns a dictionary of all the meta attributes that will need to be reset on a RestoreState
CFDictionaryRef Transform::GetAHDictForSaveState(SecTransformStringOrAttributeRef key)
{
	SecTransformMetaAttributeType types[] =
	{
		kSecTransformMetaAttributeRequired,
		kSecTransformMetaAttributeRequiresOutboundConnection,
		kSecTransformMetaAttributeDeferred,
		kSecTransformMetaAttributeStream,
		kSecTransformMetaAttributeCanCycle,
		kSecTransformMetaAttributeValue
	};
	
	CFIndex i, cnt = sizeof(types)/sizeof(SecTransformMetaAttributeType);
	CFTypeRef values[cnt];
	CFNumberRef keys[cnt];
	key = getAH(key);
	
	// NOTE: we save meta attributes that are in their "default" state on purpose because the
	// default may change in the future and we definitely want to restore the default values at
	// time of save (i.e. if "stream=1" is the 10.7 default, but "stream=0" becomes the 10.8
	// default we want to load all old transforms with stream=1, the simplest way to do that is
	// to store all values, not just non-default values)
	for(i = 0; i < cnt; ++i)
	{
		values[i] = GetMetaAttribute(key, types[i]);
		int tmp = (int)types[i];
		keys[i] = CFNumberCreate(NULL, kCFNumberIntType, &tmp);
	}
	
	static CFDictionaryValueCallBacks CFTypeOrNULL;
	static dispatch_once_t once;
	dispatch_block_t b =
	^{
		CFTypeOrNULL.version = 0;
		CFTypeOrNULL.retain = CFTypeOrNULLRetain;
		CFTypeOrNULL.release = CFTypeOrNULLRelease;
		CFTypeOrNULL.copyDescription = CFTypeOrNULLCopyDescription;
		CFTypeOrNULL.equal = CFTypeOrNULLEqual;
	};
	dispatch_once(&once, b);
	
	CFDictionaryRef ret = CFDictionaryCreate(NULL, (const void**)&keys, (const void**)&values, cnt, &kCFTypeDictionaryKeyCallBacks, &CFTypeOrNULL);
	
	for(i = 0; i < cnt; ++i)
	{
		CFRelease(keys[i]);
	}
	
	return ret;
}

// return everything that doesn't have ignore_while_externalizing set
CFDictionaryRef Transform::CopyState()
{
	CFIndex i, j, cnt = CFSetGetCount(mAttributes);
	transform_attribute *attrs[cnt];
	CFStringRef names[cnt];
	CFDictionaryRef values[cnt];
	TAGetAll(attrs);
	for(i = j = 0; i < cnt; ++i)
	{
		transform_attribute *ta = attrs[i];
		if (!ta->ignore_while_externalizing)
		{
			names[j] = ta->name;
			values[j++] = GetAHDictForSaveState(ta->name);
		}
	}
	
	CFDictionaryRef result = CFDictionaryCreate(NULL, (const void**)&names, (const void**)&values, j, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	for(i = j = 0; i < cnt; ++i)
	{
		transform_attribute *ta = attrs[i];
		if (!ta->ignore_while_externalizing)
		{
			CFRelease(values[j++]);
		}
	}
	
	return result;
}



void Transform::RestoreState(CFDictionaryRef state)
{
	CFIndex i, cnt = CFDictionaryGetCount(state);
	const void
		**keys = (const void **)alloca(sizeof(void*)*cnt),
		**values = (const void **)alloca(sizeof(void*)*cnt);

	CFDictionaryGetKeysAndValues(state, keys, values);
	
	// Open issue -- do we need to do anything to values that are already set, but are not in "state"?
	// this isn't an issue right now, which is only used on the SecTransformCopyExternalRepresentation path which starts with brand new objects,
	// it only becomes an issue if we add a ResetFromState, or use it internally in that role.
	
	for(i = 0; i < cnt; i++)
	{
		SecTransformAttributeRef ah = getAH(keys[i]);
		
		if (NULL == ah)
		{
			continue;
		}
		
		CFIndex j, meta_cnt = CFDictionaryGetCount((CFDictionaryRef)values[i]);
		const void **types = (const void**)alloca(sizeof(void*)*meta_cnt), **meta_values = (const void**)alloca(sizeof(void*)*meta_cnt);
		CFDictionaryGetKeysAndValues((CFDictionaryRef)values[i], types, meta_values);
		
		int t;
		for(j = 0; j < meta_cnt; ++j)
		{
			CFNumberGetValue((CFNumberRef)types[j], kCFNumberIntType, &t);
			if (t == kSecTransformMetaAttributeValue)
			{
				if (meta_values[j]) {
                    // SendMetaAttribute doesn't activate the callbacks 
                    SetAttribute(ah, meta_values[j]);
                }
			}
			else
			{
				CFErrorRef result = SendMetaAttribute(ah, (SecTransformMetaAttributeType)t, meta_values[j]);
				if (result)
				{
					CFRelease(result); // see <rdar://problem/8741628> Transform::RestoreState is ignoring error returns
				}
			}
		}
		
		CFErrorRef result = SendMetaAttribute(ah, kSecTransformMetaAttributeExternalize, kCFBooleanTrue);
		if (result)
		{
			CFRelease(result); // see <rdar://problem/8741628> Transform::RestoreState is ignoring error returns
		}
	}
}

GroupTransform* Transform::GetRootGroup()
{
    GroupTransform *g = mGroup;
	if (g) {
        while (g->mGroup) {
            g = g->mGroup;
        }
    } else {
        if (CFGetTypeID(this->GetCFObject()) == SecGroupTransformGetTypeID()) {
            return (GroupTransform *)this;
        }
    }
    return g;
}

CFDictionaryRef Transform::GetCustomExternalData()
{
	return NULL;
}

void Transform::SetCustomExternalData(CFDictionaryRef customData)
{
	return;
}

CFDictionaryRef Transform::Externalize(CFErrorRef* error)
{
	if (mIsActive) 
	{
		return (CFDictionaryRef)CreateSecTransformErrorRef(kSecTransformTransformIsExecuting, "The %@ transform is executing, you need to externalize it prior to execution", GetName());
	}
	
	// make arrays to hold the transforms and the connections
	__block CFMutableArrayRef transforms = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	__block CFMutableArrayRef connections = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	GroupTransform *root = GetRootGroup();
	
	CFErrorRef err = ForAllNodes(false, true, ^(Transform *t) {
        if (t != root) {
            return t->ProcessExternalize(transforms, connections);
        }
        return (CFErrorRef)NULL;
	});
	
	if (NULL != err)
	{
		// Really?  This just seems like a bad idea
		if (NULL != error)
		{
			*error = err;
		}
		return NULL;

	}
	
	// make a dictionary to hold the output
	CFMutableDictionaryRef output = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(output, EXTERN_TRANSFORM_TRANSFORM_ARRAY, transforms);
	CFDictionaryAddValue(output, EXTERN_TRANSFORM_CONNECTION_ARRAY, connections);
		
	// clean up
	CFRelease(connections);
	CFRelease(transforms);
	
	return output;
}

CFErrorRef Transform::ProcessExternalize(CFMutableArrayRef transforms, CFMutableArrayRef connections)
{	
	if (!IsExternalizable()) {
		return NULL;
	}
	
	CFDictionaryRef state = CopyState();
	if (state && CFGetTypeID(state) == CFErrorGetTypeID()) {
		return (CFErrorRef)state;
	}
	
	// make a dictionary to hold the name, type, and state of this node
	CFMutableDictionaryRef node = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(node, EXTERN_TRANSFORM_NAME, GetName());
	
	CFTypeRef type = CFStringCreateCopy(NULL, mTypeName);
	CFDictionaryAddValue(node, EXTERN_TRANSFORM_TYPE, type);
	CFRelease(type);
	
	if (state != NULL)
	{
		CFDictionaryAddValue(node, EXTERN_TRANSFORM_STATE, state);
		CFRelease(state);
	}
	
	CFDictionaryRef customItems = GetCustomExternalData();
	if (NULL != customItems)
	{
		CFDictionaryAddValue(node, EXTERN_TRANSFORM_CUSTOM_EXPORTS_DICTIONARY, customItems);
		CFRelease(customItems);
	}
	
	// append the resulting dictionary to the node list
	CFArrayAppendValue(transforms, node);
	CFRelease(node);
	
	// now walk the attribute list
	CFIndex numAttributes = CFSetGetCount(mAttributes);
	transform_attribute *attributes[numAttributes];
	TAGetAll(attributes);
	
	CFIndex i;
		
	// walk the forward links
	for (i = 0; i < numAttributes; ++i)
	{
		CFIndex arraySize = attributes[i]->connections ? CFArrayGetCount(attributes[i]->connections) : 0;
		if (arraySize != 0)
		{
			CFIndex j;
			for (j = 0; j < arraySize; ++j)
			{
				transform_attribute *ta = ah2ta((SecTransformAttributeRef)CFArrayGetValueAtIndex(attributes[i]->connections, j));
				
				if (!ta->transform->IsExternalizable()) {
					// just pretend non-externalizable transforms don't even exist.   Don't write out connections, and don't talk to them about externalizing.
					continue;
				}
				
				// add this forward connection to the array -- make a dictionary
				CFMutableDictionaryRef connection =
				CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				
				CFDictionaryAddValue(connection, EXTERN_TRANSFORM_FROM_NAME, GetName());
				CFDictionaryAddValue(connection, EXTERN_TRANSFORM_FROM_ATTRIBUTE, attributes[i]->name);
				CFDictionaryAddValue(connection, EXTERN_TRANSFORM_TO_NAME, ta->transform->GetName());
				CFDictionaryAddValue(connection, EXTERN_TRANSFORM_TO_ATTRIBUTE, ta->name);
				
				CFArrayAppendValue(connections, connection);
				CFRelease(connection);
			}
		}
	}
	
	return NULL;
}
