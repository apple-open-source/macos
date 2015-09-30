#ifndef __TRANSFORM__
#define __TRANSFORM__

#include <CoreFoundation/CFError.h>
#include "CoreFoundationBasics.h"
#include "SecTransform.h"
#include "SecCustomTransform.h"
#include <dispatch/dispatch.h>
#include "misc.h"

// Since we are doing everything in CF, we just define an
// attribute as a CFDictionary containing a value and
// a CFArray of objects which need notification when that
// value changes

// defines for transform externalization
#define EXTERN_TRANSFORM_TRANSFORM_ARRAY CFSTR("TRANSFORMS")
#define EXTERN_TRANSFORM_CONNECTION_ARRAY CFSTR("ARRAY")
#define EXTERN_TRANSFORM_CUSTOM_EXPORTS_DICTIONARY CFSTR("CUSTOM_EXPORTS")

#define EXTERN_TRANSFORM_NAME CFSTR("NAME")
#define EXTERN_TRANSFORM_TYPE CFSTR("TYPE")
#define EXTERN_TRANSFORM_STATE CFSTR("STATE")
#define EXTERN_TRANSFORM_FROM_NAME CFSTR("FROM_NAME")
#define EXTERN_TRANSFORM_FROM_ATTRIBUTE CFSTR("FROM_ATTRIBUTE")
#define EXTERN_TRANSFORM_TO_NAME CFSTR("TO_NAME")
#define EXTERN_TRANSFORM_TO_ATTRIBUTE CFSTR("TO_ATTRIBUTE")

#ifndef __clang__
#define GCC_BUG_WORKAROUND ::
#else
#define GCC_BUG_WORKAROUND
#endif


class Monitor;
typedef CFTypeRef SecMonitorRef;

struct transform_attribute {
	CFStringRef name;
	CFTypeRef value;
	CFMutableArrayRef connections;
	// NOTE: this does NOT have a reference.
	class Transform *transform;
	static CFTypeID cftype;
	
	// NOTE: NULL is a valid value to pushback, so we can't just use pushback_value==NULL for "nothing pushed back"
	// pb_empty => no value currently pushed back
	// pb_value => we have a value, we havn't presented it yet (if pushback_value==NULL we won't present again until another attribute changes)
	// pb_repush => pushed back value currently being re-processed
	// pb_presented_once => we have a value, and tried to process it and got it back again (don't present until another attribute changes)
	// 
	enum pushback_states { pb_empty, pb_value, pb_repush, pb_presented_once, pb_discard } pushback_state;
	CFTypeRef pushback_value;
	
	// (for pushback support; also need pushback state & value)
	dispatch_queue_t q;
	dispatch_semaphore_t semaphore;
	
	// This attribute needs a value set, or to have something connected to it before running the transform
	unsigned int required:1;
	// This attribute needs to have an outgoing connection before running the transform
	unsigned int requires_outbound_connection:1;
	// This attribute should not be presented to the transform until after execution starts
	unsigned int deferred:1;
	// This attribute comes in N chunks followed by a NULL 
	unsigned int stream:1;
	// This attribute should be saved when externalizing
	unsigned int ignore_while_externalizing:1;
	// Set by Transform::Connect
	unsigned int has_incoming_connection:1;
	// CustomTransform should't special case CFErrors for this attribute
	unsigned int direct_error_handling:1;
	// External sets are problematic, I think they should be disallowed full stop, but 7947393 says we need them sometimes
	unsigned int allow_external_sets:1;
	// Value has been created as a source (therefore deferred), give it special treatment
	unsigned int has_been_deferred:1;
	
	void *attribute_changed_block;
	void *attribute_validate_block;
};

typedef void (^ActivityMonitor)(CFStringRef name, CFTypeRef value);

class GroupTransform; 	// Forward reference so we do not have to include 
						// the header and break a circular dependency
class BlockMonitor;

class Transform : public CoreFoundationObject
{
	friend CFTypeRef SecTransformExecute(SecTransformRef tranformRef, CFErrorRef* errorRef);
	friend CFTypeRef SecTransformGetAttribute(SecTransformRef transformRef, CFStringRef key);
	friend class BlockMonitor;
protected:
	dispatch_queue_t mDispatchQueue, mActivationQueue;
	dispatch_group_t mActivationPending;
	CFMutableSetRef mAttributes;
	CFMutableArrayRef mPushedback;
	Boolean mIsActive;
	Boolean mIsFinalizing;
	Boolean mAlwaysSelfNotify, mProcessingPushbacks;
	GroupTransform *mGroup;
	CFErrorRef mAbortError;
	CFStringRef mTypeName;

	SecTransformAttributeRef AbortAH, DebugAH;

	Transform(CFStringRef transformType, CFStringRef CFobjectType = CFSTR("SecTransform"));
	
	transform_attribute *getTA(SecTransformStringOrAttributeRef attr, bool create_ok);
	void TAGetAll(transform_attribute **attributes);
	CFIndex GetAttributeCount();

	CFDictionaryRef GetAHDictForSaveState(SecTransformStringOrAttributeRef key);

	CFTypeRef ValueForNewAttribute(CFStringRef key, CFTypeRef value);
	CFMutableDictionaryRef AddNewAttribute(CFStringRef key, CFTypeRef value);
	CFErrorRef SetAttributeNoCallback(SecTransformStringOrAttributeRef key, CFTypeRef value);

	CFErrorRef ProcessExecute(CFStringRef &outputAttached, SecMonitorRef monitor);
	typedef void (^AccumulateDictonary)(CFDictionaryRef d);
	CFErrorRef ProcessExternalize(CFMutableArrayRef transforms, CFMutableArrayRef connections);

	void FinalizeForClang();
	
	virtual void Finalize();
	// subclasses with non-trivial finalization can implement this (default: delete this)
	virtual void FinalizePhase2();
    // subclasses that want to reject some connections can use this
    virtual bool validConnectionPoint(CFStringRef attributeName);
	
	void try_pushbacks();

	void Initialize();

	void ActivateInputs();

	virtual std::string DebugDescription();
	
	typedef CFErrorRef (^TransformOperation)(Transform*);
	typedef void (^TransformAsyncOperation)(Transform*);
    CFErrorRef ForAllNodes(bool parallel, bool includeOwningGroup, TransformOperation op);
	
	CFErrorRef TraverseTransform(CFMutableSetRef visited, TransformOperation t);


	CFErrorRef SendAttribute(SecTransformStringOrAttributeRef key, CFTypeRef value);
	CFErrorRef SendMetaAttribute(SecTransformStringOrAttributeRef key, SecTransformMetaAttributeType type, CFTypeRef value);
	
    // Abort all transforms in this transform's RootGroup, including this transform
	virtual void AbortAllTransforms(CFTypeRef error);
    // Abort just this transform (and maybe schedule a later call to AbortAllTransforms), should only be
    // called via AbortAllTransforms
	virtual void AbortJustThisTransform(CFErrorRef abortMsg);

	void phase3Activation();
    void DoPhase3Activation();

	bool HasNoInboundConnections();
	bool HasNoOutboundConnections();

private:
	CFErrorRef ExecuteOperation(CFStringRef &outputAttached, SecMonitorRef output, dispatch_queue_t phase2, dispatch_queue_t phase3);
	SecTransformAttributeRef makeAH(transform_attribute *ta);
	
public:
	
	static CFTypeID GetCFTypeID();

	// these functions are overloaded to implement the functionality of your transform
	virtual ~Transform();
	
	// this is called when one of your attributes (e.g. input) changes
	virtual void AttributeChanged(SecTransformAttributeRef ah, CFTypeRef value);
	// this is for backwards compatibility only (XXX: convert all existing Transform subclasses to not use it then remove)
	virtual void AttributeChanged(CFStringRef name, CFTypeRef value);

	// overload to return true if your transform can be externalized (generally true unless you are a monitor)
	virtual bool IsExternalizable();

	// Base implementation saves all attributes that have kSecTransformMetaAttributeExternalize TRUE (which is the default).
	// If that isn't useful for your transform overload to return a CFDictionary that contains the state of
	// your transform.  Values returned should be serializable.  Remember that this state will be restored
	// before SecTransformExecute is called.  Do not include the transform name in your state (this will be
	// done for you by SecTransformCopyExternalRep).
	virtual CFDictionaryRef CopyState();
	
	// overload to restore the state of your transform
	virtual void RestoreState(CFDictionaryRef state);
	virtual void SetCustomExternalData(CFDictionaryRef customData);

	virtual Boolean TransformCanExecute();
	virtual CFErrorRef TransformStartingExecution();
	
	SecTransformAttributeRef getAH(SecTransformStringOrAttributeRef attr, bool create_ok =true, bool create_undesrscore_ok =false);
	CFArrayRef GetAllAH();

	CFStringRef GetName();
	
	// Output debugging information if the DEBUG attribute is set for this transform
	void Debug(const char *fmt, ...);
	
	CFErrorRef RefactorErrorToIncludeAbortingTransform(CFErrorRef sourceError);
    
public:
	CFErrorRef Connect(GroupTransform *group, Transform* destinationTransform, CFStringRef myKey, CFStringRef hisKey);
	CFErrorRef Disconnect(Transform* destinationTransform, CFStringRef myKey, CFStringRef hisKey);	
	
	CFErrorRef ExternalSetAttribute(SecTransformStringOrAttributeRef key, CFTypeRef value);
	CFErrorRef SetAttribute(SecTransformStringOrAttributeRef key, CFTypeRef value);
	CFTypeRef GetAttribute(SecTransformStringOrAttributeRef key);
	CFTypeRef GetMetaAttribute(SecTransformStringOrAttributeRef key, SecTransformMetaAttributeType type);
	
	CFErrorRef Pushback(SecTransformAttributeRef ah, CFTypeRef value);
	
	void Do(SecTransformAttributeRef name, CFTypeRef value);	
	
	CFTypeRef Execute(dispatch_queue_t deliveryQueue, SecMessageBlock deliveryBlock, CFErrorRef* errorRef);
	
	// set to get notified every time this transform does something -- used for debugging
	void SetActivityMonitor(ActivityMonitor am);
	
	virtual CFDictionaryRef Externalize(CFErrorRef *error);
	
    // Returns NULL if not in a group; can return this
    GroupTransform* GetRootGroup();

	friend class GroupTransform;
	friend Transform::TransformOperation makeIdleOp(dispatch_group_t idle_group);
	
	void SetGroup(GroupTransform* group) {mGroup = group;}
	CFDictionaryRef GetCustomExternalData();
};


inline struct transform_attribute *ah2ta(SecTransformAttributeRef ah) {
	// CF stores our data just after the CFRuntimeBase, we just have a single pointer there.
	return *(struct transform_attribute **)(1 + (CFRuntimeBase*)ah);
}

#endif
