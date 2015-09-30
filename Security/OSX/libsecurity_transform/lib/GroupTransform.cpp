#include "GroupTransform.h"
#include "Utilities.h"
#include "misc.h"
#include <string>
#include <libkern/OSAtomic.h>

using namespace std;

CFStringRef kSecGroupTransformType = CFSTR("GroupTransform");

GroupTransform::GroupTransform() : Transform(kSecGroupTransformType, kSecGroupTransformType)
{
	mMembers = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	mAllChildrenFinalized = dispatch_group_create();
    mPendingStartupActivity = dispatch_group_create();
}

// Invoked by Transform::Finalize
void GroupTransform::FinalizePhase2()
{
	// Any time afer mMembers is released this can be deleted, so we need a local.
	CFArrayRef members = this->mMembers;
    dispatch_group_notify(mPendingStartupActivity, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        if (mMembers) {
            this->mMembers = NULL;
            CFRelease(members);
        }
    });
    
	// Delay the final delete of the group until all children are gone (and thus unable to die while referencing us).
	dispatch_group_notify(mAllChildrenFinalized, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
		delete this;
	});
}

void GroupTransform::StartingExecutionInGroup()
{
    this->mIsActive = true;
    dispatch_group_enter(mPendingStartupActivity);
    dispatch_group_enter(mAllChildrenFinalized);
}

void GroupTransform::StartedExecutionInGroup(bool succesful)
{
    dispatch_group_leave(mPendingStartupActivity);
    dispatch_group_leave(mAllChildrenFinalized);
}

bool GroupTransform::validConnectionPoint(CFStringRef attributeName)
{
    // We don't want connections to/from unexported attributes
    return NULL != this->getAH(attributeName, false);
}

GroupTransform::~GroupTransform()
{
	// mMembers already released (via FinalizePhase2)
	dispatch_release(mAllChildrenFinalized);
    dispatch_release(mPendingStartupActivity);
}

void GroupTransform::ChildStartedFinalization(Transform *child)
{
    // child started finalizing before the group??  Likely client over release.
    (void)transforms_assume(this->mIsFinalizing);
	dispatch_group_leave(mAllChildrenFinalized);
}

CFTypeRef GroupTransform::Make()
{
	return CoreFoundationHolder::MakeHolder(kSecGroupTransformType, new GroupTransform());
}

static CFComparisonResult tr_cmp(const void *val1, const void *val2, void *context)
{
	return (intptr_t)val1 - (intptr_t)val2;
}

bool GroupTransform::HasMember(SecTransformRef member)
{
	// find the transform in the group
	CFIndex numMembers = CFArrayGetCount(mMembers);
	CFIndex i;
	
	for (i = 0; i < numMembers; ++i)
	{
		if (CFArrayGetValueAtIndex(mMembers, i) == member)
		{
			return true;
		}
	}
	
	return false;
}


	
void GroupTransform::RemoveMemberFromGroup(SecTransformRef member)
{
	// find the transform in the group and remove it
	// XXX: this would be a lot faster if it used CFArrayBSearchValues
	CFIndex numMembers = CFArrayGetCount(mMembers);
	CFIndex i;
	
	for (i = 0; i < numMembers; ++i)
	{
		if (CFArrayGetValueAtIndex(mMembers, i) == member)
		{
			// removing the item will release it, so we don't need an explicit release
			CFArrayRemoveValueAtIndex(mMembers, i);
			numMembers = CFArrayGetCount(mMembers);
			dispatch_group_leave(mAllChildrenFinalized);
		}
	}
}

void GroupTransform::AddAllChildrenFinalizedCallback(dispatch_queue_t run_on, dispatch_block_t callback)
{
	dispatch_group_notify(mAllChildrenFinalized, run_on, callback);
}


void GroupTransform::AddMemberToGroup(SecTransformRef member)
{
	// set a backlink to this group in the child (used for abort and other purposes)
	Transform* transform = (Transform*) CoreFoundationHolder::ObjectFromCFType(member);
	// XXX: it seems like we should be able to ensure that we are the only caller to SetGroup, and that if the group is set to us we could skip this search... 
    transform->SetGroup(this);
    if (transform == this) {
        // We don't want to be in our own membership list, at a minimum
        // that makes reference counts cranky.
        return;
    }
	
	// check to make sure that member is not already in the group (the bsearch code is a bit more complex, but cuts run time for the 8163542 test from about 40 minutes to under a minute)
	CFIndex numMembers = CFArrayGetCount(mMembers);
	CFRange range = {0, numMembers};
	CFIndex at = CFArrayBSearchValues(mMembers, range, member, tr_cmp, NULL);
	SecTransformRef candiate = (at < numMembers) ? CFArrayGetValueAtIndex(mMembers, at) : NULL;
	if (member == candiate) {
		return;
	}
	
	CFArrayInsertValueAtIndex(mMembers, at, member);
	dispatch_group_enter(mAllChildrenFinalized);
}



std::string GroupTransform::DebugDescription()
{
	return Transform::DebugDescription() + ": GroupTransform";
}



class GroupTransformFactory : public TransformFactory
{
public:
	GroupTransformFactory();
	
	virtual CFTypeRef Make();
};



TransformFactory* GroupTransform::MakeTransformFactory()
{
	return new GroupTransformFactory();
}



GroupTransformFactory::GroupTransformFactory() : TransformFactory(kSecGroupTransformType)
{
}



CFTypeRef GroupTransformFactory::Make()
{
	return GroupTransform::Make();
}



CFTypeID GroupTransform::GetCFTypeID()
{
	return CoreFoundationObject::FindObjectType(kSecGroupTransformType);
}




SecTransformRef GroupTransform::FindFirstTransform()
{
	// look for a transform that has no connections to INPUT (prefer ones where INPUT is required)
	CFRange range;
	range.location = 0;
	range.length = CFArrayGetCount(mMembers);
	SecTransformRef items[range.length];
	SecTransformRef maybe = NULL;
	
	CFArrayGetValues(mMembers, range, items);
	
	CFIndex i;
	for (i = 0; i < range.length; ++i)
	{
		SecTransformRef tr = (SecTransformRef) items[i];
		Transform* t = (Transform*) CoreFoundationHolder::ObjectFromCFType(tr);
		SecTransformAttributeRef in = getAH(kSecTransformInputAttributeName, false);
		if (!t->GetMetaAttribute(in, kSecTransformMetaAttributeHasInboundConnection)) {
			maybe = tr;
			if (t->GetMetaAttribute(in, kSecTransformMetaAttributeRequired)) {
				return tr;
			}
		}
	}
	
	return maybe;
}


SecTransformRef GroupTransform::GetAnyMember()
{
	if (CFArrayGetCount(mMembers)) {
		return CFArrayGetValueAtIndex(mMembers, 0);
	} else {
		return NULL;
	}
}

// Pretty horrible kludge -- we will do Very Bad Things if someone names their transform <anything>Monitor
SecTransformRef GroupTransform::FindMonitor()
{
	// list all transforms in the group
	CFRange range;
	range.location = 0;
	range.length = CFArrayGetCount(mMembers);
	SecTransformRef items[range.length];
	CFArrayGetValues(mMembers, range, items);
	
	// check each item to see if it is a monitor
	CFIndex i;
	for (i = 0; i < range.length; ++i)
	{
		SecTransformRef tr = (SecTransformRef) items[i];
		Transform* t = (Transform*) CoreFoundationHolder::ObjectFromCFType(tr);
		
		if (CFStringHasSuffix(t->mTypeName, CFSTR("Monitor"))) {
			return tr;
		}
	}
	
	return NULL;
}



SecTransformRef GroupTransform::FindLastTransform()
{
	// If there's a monitor attached to this transform, the last transform is
	// the transform that points to it.  Otherwise, the last transform is the
	// one that has nothing connected to its output attribute and said attribute
	// is marked as requiring an outbound connection.
	
	// WARNING: if this function and Transform::ExecuteOperation disagree about
	// where to attach a monitor things could get funky.   It would be very nice
	// to implement one of these in terms of the other.
	
	SecTransformRef lastOrMonitor = FindMonitor(); // this will either be NULL or the monitor.
												   // We win either way.
	
	// list all transforms in the group
	CFRange range;
	range.location = 0;
	range.length = CFArrayGetCount(mMembers);
	SecTransformRef items[range.length];
	CFArrayGetValues(mMembers, range, items);
	
	// if the output attribute of a transform matches our target, we win
	CFIndex i;
	for (i = 0; i < range.length; ++i)
	{
		Transform* tr = (Transform*) CoreFoundationHolder::ObjectFromCFType(items[i]);
		
		// get the output attribute for the transform
		transform_attribute* ta = tr->getTA(kSecTransformOutputAttributeName, false);
		
		if (lastOrMonitor == NULL)
		{
			if (ta->requires_outbound_connection && (ta->connections == NULL || (CFArrayGetCount(ta->connections) == 0)))
			{
				// this a transform with an unattached OUTPUT with RequiresOutboundConnection true
				return items[i];
			}
		} else {
			if (ta->connections) {
				// get all of the connections for that attribute, and see if one of them is the monitor
				CFRange connectionRange;
				connectionRange.location = 0;
				connectionRange.length = CFArrayGetCount(ta->connections);
				SecTransformAttributeRef attributeHandles[connectionRange.length];
				CFArrayGetValues(ta->connections, connectionRange, attributeHandles);
				
				CFIndex j;
				for (j = 0; j < connectionRange.length; ++j)
				{
					transform_attribute* ta = ah2ta(attributeHandles[j]);
					if (ta->transform->GetCFObject() == lastOrMonitor)
					{
						return items[i];
					}
				}
			}
		}
	}
	
	// this chain is seriously whacked!!!
	return NULL;
}

SecTransformRef GroupTransform::FindByName(CFStringRef name)
{
	__block SecTransformRef ret = NULL;
	static CFErrorRef early_return = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EEXIST, NULL);
	
	ForAllNodes(true, true, ^(Transform *t){
		if (!CFStringCompare(name, t->GetName(), 0)) {
			ret = t->GetCFObject();
			return early_return;
		}
		return (CFErrorRef)NULL;
	});
	
	return ret;
}

CFDictionaryRef GroupTransform::Externalize(CFErrorRef* error)
{	
	return NULL;
}

// Visit all children once.   Unlike ForAllNodes there is no way to early exit, nor a way to return a status.
// Returns when all work is scheduled, use group to determine completion of work.
// See also ForAllNodes below.
void GroupTransform::ForAllNodesAsync(bool opExecutesOnGroups, dispatch_group_t group, Transform::TransformAsyncOperation op)
{
    dispatch_group_enter(group);
	CFIndex lim = mMembers ? CFArrayGetCount(mMembers) : 0;
	
	if (opExecutesOnGroups)
	{
		dispatch_group_async(group, mDispatchQueue, ^{
            op(this);
        });
	}
	
	for(CFIndex i = 0; i < lim; ++i)
	{
		SecTransformRef tr = CFArrayGetValueAtIndex(mMembers, i);
        Transform *t = (Transform*)CoreFoundationHolder::ObjectFromCFType(tr);
        
        if (CFGetTypeID(tr) == SecGroupTransformGetTypeID()) {
            GroupTransform *g = (GroupTransform*)t;
            g->ForAllNodesAsync(true, group, op);
        } else {
            dispatch_group_async(group, t->mDispatchQueue, ^{ 
                op(t);
            });
        }
	}
    dispatch_group_leave(group);
}

// Visit all nodes once (at most), attempts to stop if any op
// returns non-NULL (if parallel is true in flight ops are not
// stopped).   Returns when all work is complete, and returns
// the "first" non-NULL op value (or NULL if all ops returned
// NULL).  Uses ForAllNodes below to do the dirty work.
CFErrorRef GroupTransform::ForAllNodes(bool parallel, bool opExecutesOnGroups, Transform::TransformOperation op)
{
    dispatch_group_t inner_group = dispatch_group_create();
    
    CFErrorRef err = NULL;
    RecurseForAllNodes(inner_group, &err, parallel, opExecutesOnGroups, op);
    
    dispatch_group_wait(inner_group, DISPATCH_TIME_FOREVER);
    dispatch_release(inner_group);
    
    return err;
}

// Visit all children once (at most), because groups can appear in
// multiple other groups use visitedGroups (protected by
// rw_queue) to avoid multiple visits.   Will stop if an op
// returns non-NULL.
// (Used only by ForAllNodes above)
void GroupTransform::RecurseForAllNodes(dispatch_group_t group, CFErrorRef *err_, bool parallel, bool opExecutesOnGroups, Transform::TransformOperation op)
{
    __block CFErrorRef *err = err_;
    void (^set_error)(CFErrorRef new_err) = ^(CFErrorRef new_err) {
        if (new_err) {
	    if (!OSAtomicCompareAndSwapPtrBarrier(NULL, (void *)new_err, (void**)err)) {
                CFRelease(new_err);
            }
        }
    };
    void (^runOp)(Transform *t) = ^(Transform *t){
        if (parallel) {
            dispatch_group_async(group, t->mDispatchQueue, ^{ 
                set_error(op(t));
            });
        } else {
            set_error(op(t));
        }
    };

    dispatch_group_enter(group);
	if (opExecutesOnGroups) {
        runOp(this);
	}
	
    
	CFIndex i, lim = CFArrayGetCount(mMembers);
	
	for(i = 0; i < lim && !*err; ++i) {
		SecTransformRef tr = CFArrayGetValueAtIndex(mMembers, i);
		Transform *t = (Transform*)CoreFoundationHolder::ObjectFromCFType(tr);
        
        if (CFGetTypeID(tr) == SecGroupTransformGetTypeID()) {
            GroupTransform *g = (GroupTransform*)t;
            g->RecurseForAllNodes(group, err, parallel, opExecutesOnGroups, op);
        } else {
            runOp(t);
        }
	}

    dispatch_group_leave(group);
}

// Return a dot (GraphViz) description of the group.
// For debugging use.   Exact content and style may
// change.   Currently all transforms and attributes
// are displayed, but only string values are shown
// (and no meta attributes are indicated).
CFStringRef GroupTransform::DotForDebugging()
{
    __block CFMutableStringRef result = CFStringCreateMutable(NULL, 0);
    CFStringAppend(result, CFSTR("digraph \"G\" {\n"));
    
    dispatch_queue_t collect_nodes = dispatch_queue_create("dot-node-collector", NULL);
    dispatch_group_t complete_nodes = dispatch_group_create();
    dispatch_queue_t collect_connections = dispatch_queue_create("dot-connection-collector", NULL);
    dispatch_group_t complete_connections = dispatch_group_create();
    // Before we reference a node we need it to be declared in the correct
    // graph cluster, so we defer all the connection output until we
    // have all the nodes defined.
    dispatch_suspend(collect_connections);
    
    
    this->ForAllNodesAsync(true, complete_nodes, ^(Transform *t) {
        CFStringRef name = t->GetName();
        __block CFMutableStringRef group_nodes_out = CFStringCreateMutable(NULL, 0);
        __block CFMutableStringRef group_connections_out = CFStringCreateMutable(NULL, 0);
        CFStringRef line_out = CFStringCreateWithFormat(NULL, NULL, CFSTR("\tsubgraph \"cluster_%@\" {\n"), name);
        CFStringAppend(group_nodes_out, line_out);
        CFRelease(line_out);
        line_out = NULL;
        
        CFIndex n_attributes = t->GetAttributeCount();
        transform_attribute **attributes = (transform_attribute **)alloca(n_attributes * sizeof(transform_attribute *));
        t->TAGetAll(attributes);
        CFMutableArrayRef most_dot_names = CFArrayCreateMutable(NULL, n_attributes -1, &kCFTypeArrayCallBacks);
        for(int i = 0; i < n_attributes; i++) {
            CFStringRef label = attributes[i]->name;
            if (attributes[i]->value) {
                if (CFGetTypeID(attributes[i]->value) == CFStringGetTypeID()) {
                    label = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@=%@"), attributes[i]->name, attributes[i]->value);
                }
            }
            if (!label) {
                label = CFStringCreateCopy(NULL, attributes[i]->name);
            }
            
            CFStringRef dot_node_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("\"%@#%@\""), name, attributes[i]->name);
            if (CFStringCompare(CFSTR("NAME"), label, 0)) {
                CFArrayAppendValue(most_dot_names, dot_node_name);
            }
            line_out = CFStringCreateWithFormat(NULL, NULL, CFSTR("\t\t%@ [shape=plaintext, label=\"%@\"]\n"), dot_node_name, label);
            CFStringAppend(group_nodes_out, line_out);
            CFRelease(line_out);
            line_out = NULL;
            CFRelease(label);
            
            CFIndex n_connections = attributes[i]->connections ? CFArrayGetCount(attributes[i]->connections) : 0;
            for(int j = 0; j < n_connections; j++) {
                transform_attribute *connected_to = ah2ta(CFArrayGetValueAtIndex(attributes[i]->connections, j));
                line_out = CFStringCreateWithFormat(NULL, NULL, CFSTR("\t%@ -> \"%@#%@\"\n"), dot_node_name, connected_to->transform->GetName(), connected_to->name);
                CFStringAppend(group_connections_out, line_out);
                CFRelease(line_out);
            }
        }
        
        line_out = CFStringCreateWithFormat(NULL, NULL, CFSTR("\t\t\"%@#NAME\" -> { %@ } [style=invis]\n\t}\n"), name, CFStringCreateByCombiningStrings(NULL, most_dot_names, CFSTR(" ")));
        CFStringAppend(group_nodes_out, line_out);
        CFRelease(line_out);
        if (t->mGroup) {
            line_out = CFStringCreateWithFormat(NULL, NULL, CFSTR("\t\"%@#NAME\" -> \"%@#NAME\" [style=dotted,weight=5]\n"), name, t->mGroup->GetName());
            CFStringAppend(group_connections_out, line_out);
            CFRelease(line_out);
        }
        line_out = NULL;
        
        dispatch_async(collect_nodes, ^(void) {
            CFStringAppend(result, group_nodes_out);
            CFRelease(group_nodes_out);
        });
        dispatch_group_async(complete_connections, collect_connections, ^(void) {
            // We don't really need to append to result on the collect_nodes queue
            // because we happen to know no more work is going on on the collect_nodes
            // queue, but if that ever changed we would have a hard to track down bug...
            dispatch_async(collect_nodes, ^(void) {
                CFStringAppend(result, group_connections_out);
                CFRelease(group_connections_out);
            });
        });
    });
    
    dispatch_group_wait(complete_nodes, DISPATCH_TIME_FOREVER);
    dispatch_release(complete_nodes);
    dispatch_resume(collect_connections);
    dispatch_release(collect_connections);
    dispatch_group_wait(complete_connections, DISPATCH_TIME_FOREVER);
    dispatch_release(complete_connections);
    
    dispatch_sync(collect_nodes, ^{
        CFStringAppend(result, CFSTR("}\n"));
    });
    dispatch_release(collect_nodes);
    return result;
}
