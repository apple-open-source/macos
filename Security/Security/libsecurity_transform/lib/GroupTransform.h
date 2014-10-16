#ifndef __GROUP_TRANSFORM__
#define __GROUP_TRANSFORM__


#include "Transform.h"
#include "TransformFactory.h"

extern CFStringRef kSecGroupTransformType;

class GroupTransform : public Transform
{
protected:
	std::string DebugDescription();
	virtual void FinalizePhase2();
    virtual bool validConnectionPoint(CFStringRef attributeName);
	GroupTransform();
	CFMutableArrayRef mMembers;
	dispatch_group_t mAllChildrenFinalized;
    dispatch_group_t mPendingStartupActivity;

    void RecurseForAllNodes(dispatch_group_t group, CFErrorRef *errorOut, bool parallel, bool opExecutesOnGroups, Transform::TransformOperation op);
    
public:
	virtual ~GroupTransform();

	static CFTypeRef Make();
	static TransformFactory* MakeTransformFactory();
	
	static CFTypeID GetCFTypeID();
	
	void AddMemberToGroup(SecTransformRef member);
	void RemoveMemberFromGroup(SecTransformRef member);
	bool HasMember(SecTransformRef member);
	
	void AddAllChildrenFinalizedCallback(dispatch_queue_t run_on, dispatch_block_t callback);
	void ChildStartedFinalization(Transform *child);

	SecTransformRef FindFirstTransform();		// defined as the transform to which input is attached
	SecTransformRef FindLastTransform();		// defined as the transform to which the monitor is attached
	SecTransformRef FindMonitor();
	SecTransformRef GetAnyMember();
	
	SecTransformRef FindByName(CFStringRef name);
    
    // A group should delay destruction while excution is starting
    void StartingExecutionInGroup();
    void StartedExecutionInGroup(bool succesful);
	
	virtual CFDictionaryRef Externalize(CFErrorRef* error);
	
    CFErrorRef ForAllNodes(bool parallel, bool opExecutesOnGroups, Transform::TransformOperation op);
	void ForAllNodesAsync(bool opExecutesOnGroups, dispatch_group_t group, Transform::TransformAsyncOperation op);

    CFStringRef DotForDebugging();
};



#endif
