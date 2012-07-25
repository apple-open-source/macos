/*
 *  SecGroupTransform.cpp
 *  libsecurity_transform
 *
 *  Created by ohjelmoija on 3/31/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "SecGroupTransform.h"
#include "SecTransformInternal.h"
#include "GroupTransform.h"

SecTransformRef SecGroupTransformFindLastTransform(SecGroupTransformRef groupTransform)
{
	GroupTransform* gt = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(groupTransform);
	return gt->FindLastTransform();
}



SecTransformRef SecGroupTransformFindMonitor(SecGroupTransformRef groupTransform)
{
	GroupTransform* gt = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(groupTransform);
	return gt->FindMonitor();
}



bool SecGroupTransformHasMember(SecGroupTransformRef groupTransform, SecTransformRef transform)
{
	GroupTransform* gt = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(groupTransform);
	return gt->HasMember(transform);
}


