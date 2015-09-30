#include "SecTransform.h"
#include "SecTransformInternal.h"

#include "Transform.h"
#include "Utilities.h"
#include "TransformFactory.h"
#include "GroupTransform.h"
#include "c++utils.h"
#include "SecCollectTransform.h"


#include <string>

using namespace std;

const CFStringRef kSecTransformInputAttributeName = CFSTR("INPUT");
const CFStringRef kSecTransformOutputAttributeName = CFSTR("OUTPUT");
const CFStringRef kSecTransformDebugAttributeName = CFSTR("DEBUG");
const CFStringRef kSecTransformTransformName = CFSTR("NAME");
//const CFStringRef kSecTransformErrorTransform = CFSTR("TRANSFORM");
const CFStringRef kSecTransformErrorDomain = CFSTR("com.apple.security.transforms.error");
const CFStringRef kSecTransformAbortAttributeName = CFSTR("ABORT");

CFErrorRef SecTransformConnectTransformsInternal(SecGroupTransformRef groupRef,
							        SecTransformRef sourceTransformRef,
									CFStringRef sourceAttributeName,
									SecTransformRef destinationTransformRef,
									CFStringRef destinationAttributeName)
{
	Transform* destination = (Transform*) CoreFoundationHolder::ObjectFromCFType(destinationTransformRef);
	Transform* source = (Transform*) CoreFoundationHolder::ObjectFromCFType(sourceTransformRef);

	GroupTransform* group = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(groupRef);
	CFErrorRef temp = source->Connect(group, destination, destinationAttributeName, sourceAttributeName);
	return temp;
}


CFErrorRef SecTransformDisconnectTransforms(SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
											SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName)
{
	Transform* destination = (Transform*) CoreFoundationHolder::ObjectFromCFType(destinationTransformRef);
	Transform* source = (Transform*) CoreFoundationHolder::ObjectFromCFType(sourceTransformRef);
	return source->Disconnect(destination, sourceAttributeName, destinationAttributeName);
}

SecGroupTransformRef SecTransformCreateGroupTransform()
{
	return (SecGroupTransformRef) GroupTransform::Make();
}

SecGroupTransformRef SecTransformConnectTransforms(SecTransformRef sourceTransformRef,
						   CFStringRef sourceAttributeName,
						   SecTransformRef destinationTransformRef,
				 		   CFStringRef destinationAttributeName,
						   SecGroupTransformRef group,
						   CFErrorRef *error)
{
	if (group == NULL)
	{
		if (error)
		{
			*error = CreateSecTransformErrorRef(kSecTransformErrorMissingParameter, "Group must not be NULL.");
		}

		return NULL;
	}

	if (destinationAttributeName == NULL)
	{
		destinationAttributeName = kSecTransformInputAttributeName;
	}

	if (sourceAttributeName == NULL)
	{
		sourceAttributeName = kSecTransformOutputAttributeName;
	}

	GroupTransform* gtr = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(group);

	CFErrorRef temp = SecTransformConnectTransformsInternal(gtr->GetCFObject(),
			sourceTransformRef, sourceAttributeName,
			destinationTransformRef, destinationAttributeName);

	if (error)
	{
		*error = temp;
	}

	if (temp) // an error happened?
	{
		return NULL;
	}
	else
	{
		return group;
	}
}



Boolean SecTransformSetAttribute(SecTransformRef transformRef,
								CFStringRef key,
								CFTypeRef value,
								CFErrorRef *error)
{
	Boolean result = false; // Guilty until proven
	Transform* transform = (Transform*) CoreFoundationHolder::ObjectFromCFType(transformRef);

	if (CFGetTypeID(transformRef) == GroupTransform::GetCFTypeID() && !transform->getAH(key, false))
	{
		if (error)
		{
			*error = CreateSecTransformErrorRef(kSecTransformOperationNotSupportedOnGroup, "SecTransformSetAttribute on non-exported attribute: %@ (exported attributes are: %@).", key, transform->GetAllAH());
		}

		return result;
	}

	// if the caller is setting the abort attribute, a value must be supplied
	if (NULL == value && CFStringCompare(key, kSecTransformAbortAttributeName, 0) == kCFCompareEqualTo)
	{
		if (error)
		{
            // XXX:  "a parameter"?  It has one: NULL.  What it requires is a non-NULL value.
			*error = CreateSecTransformErrorRef(kSecTransformInvalidArgument, "ABORT requires a parameter.");
		}

		return result;
	}

	CFErrorRef temp = transform->ExternalSetAttribute(key, value);
	result = (temp == NULL);
	if (error)
	{
		*error = temp;
	}
	else
	{
		if (temp)
		{
			CFRelease(temp);
		}
	}

	return result;
}



CFTypeRef SecTransformGetAttribute(SecTransformRef transformRef,
									CFStringRef key)
{
	// if the transform is a group, we really want to operation on its first object
	if (CFGetTypeID(transformRef) == GroupTransform::GetCFTypeID())
	{
		return NULL;
	}

	Transform* transform = (Transform*) CoreFoundationHolder::ObjectFromCFType(transformRef);
	if (transform->mIsActive) {
		return CreateSecTransformErrorRef(kSecTransformTransformIsExecuting, "Can not get the value of attributes during execution (attempt to fetch %@/%@)", transform->GetName(), key);
	}
	return transform->GetAttribute(key);
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static inline GroupTransform* MakeGroupTransformFromTransformRef(SecTransformRef tr)
{
	GroupTransform* gt = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(tr);
	return gt;
}
#pragma clang diagnostic pop

static CFTypeRef InternalSecTransformExecute(SecTransformRef transformRef,
							CFErrorRef* errorRef,
							dispatch_queue_t deliveryQueue,
							SecMessageBlock deliveryBlock)
{
	if (NULL == transformRef || (deliveryBlock && !deliveryQueue))
	{
		CFErrorRef localError = CFErrorCreate(kCFAllocatorDefault, kSecTransformErrorDomain,
			kSecTransformInvalidArgument, NULL);

		if (NULL != errorRef)
		{
			*errorRef = localError;
		}
		else
		{
			CFRelease(localError);
		}

		return (CFTypeRef)NULL;
	}

	// if our transform is a group, connect to its first member instead
	if (CFGetTypeID(transformRef) == GroupTransform::GetCFTypeID())
	{
		GroupTransform* gtsrc = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(transformRef);
		transformRef = gtsrc->GetAnyMember();
	}

	Transform* transform = (Transform*) CoreFoundationHolder::ObjectFromCFType(transformRef);
	return transform->Execute(deliveryQueue, deliveryBlock, errorRef);
}

CFTypeRef SecTransformExecute(SecTransformRef transformRef, CFErrorRef* errorRef)
{
	if (NULL == transformRef)
	{
		if (errorRef)
		{
			*errorRef = CreateSecTransformErrorRef(kSecTransformInvalidArgument, "NULL transform can not be executed");
		}
		return NULL;
	}

	Transform* transform = (Transform*) CoreFoundationHolder::ObjectFromCFType(transformRef);

	// transform->Execute will check this, but by then we have attached a collector which causes all manner of issues.
	if (transform->mIsActive)
	{
		if (errorRef)
		{
			*errorRef = CreateSecTransformErrorRef(kSecTransformTransformIsExecuting, "The %@ transform has already executed, it may not be executed again.", transform->GetName());
		}
		return NULL;
	}

	SecTransformRef collectTransform = transforms_assume(SecCreateCollectTransform(errorRef));
	SecGroupTransformRef theGroup = NULL;
	Boolean releaseTheGroup = false;
	GroupTransform* myGroup = NULL;
	Boolean needConnection = true;

	// Sniff the type of the transformRef to see if it is a group
	if (SecGroupTransformGetTypeID() == CFGetTypeID(transformRef))
	{
		theGroup = (SecGroupTransformRef)transformRef;
	}
	else
	{
		// Ok TransformRef is a TransformRef so get's it group

		myGroup = transform->mGroup;

		if (NULL == myGroup)
		{
			theGroup = SecTransformCreateGroupTransform();
			if (NULL == theGroup)
			{
				if (NULL != errorRef)
				{
					*errorRef = GetNoMemoryErrorAndRetain();
				}

				return (CFTypeRef)NULL;

			}

			releaseTheGroup = true;

			SecGroupTransformRef connectResult =
				SecTransformConnectTransforms(transformRef,
										  kSecTransformOutputAttributeName,
										  collectTransform,
										  kSecTransformInputAttributeName,
										  theGroup, errorRef);

			if (NULL == connectResult)
			{
				return (CFTypeRef)NULL;
			}

			needConnection = false;

		}
		else
		{
			theGroup = (SecGroupTransformRef)myGroup->GetCFObject();
		}
	}

	if (NULL == theGroup || (SecGroupTransformGetTypeID() != CFGetTypeID(theGroup)))
	{
		if (NULL != errorRef)
		{
			*errorRef = GetNoMemoryErrorAndRetain();
		}

		return (CFTypeRef)NULL;

	}


	if (needConnection)
	{
		// Connect the collectTransform to the group
		myGroup = ((GroupTransform*)CoreFoundationHolder::ObjectFromCFType(theGroup))->GetRootGroup();
        if (NULL == myGroup)
        {
            if (NULL != errorRef)
            {
                *errorRef = GetNoMemoryErrorAndRetain();
            }

            return (CFTypeRef)NULL;
        }

		SecTransformRef outputTransform = myGroup->FindLastTransform();

		SecGroupTransformRef connectResult =
		SecTransformConnectTransforms(outputTransform,
									  kSecTransformOutputAttributeName,
									  collectTransform,
									  kSecTransformInputAttributeName,
									  myGroup->GetCFObject(), errorRef);

		if (NULL == connectResult)
		{
			CFRelease(collectTransform);
			if (releaseTheGroup)
			{
				CFRelease(theGroup);
			}
			return (CFTypeRef)NULL;
		}
	}

	__block CFTypeRef myResult = NULL;
	dispatch_semaphore_t mySem = dispatch_semaphore_create(0L);
	dispatch_queue_t myQueue = dispatch_queue_create("com.apple.security.sectransfrom.SecTransformExecute", NULL);
	SecMessageBlock myBlock = ^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
	{
		if (NULL != error)
		{
			if (NULL != errorRef)
			{
				CFRetain(error);
				*errorRef = error;
			}

			if (NULL != myResult)
			{
				CFRelease(myResult);
				myResult = NULL;
			}
		}

		if (NULL != message)
		{
			myResult = message;
			CFRetain(myResult);
		}

		if (isFinal)
		{
			dispatch_semaphore_signal(mySem);
		}
	};

	SecTransformExecuteAsync(theGroup, myQueue, myBlock);
	dispatch_semaphore_wait(mySem, DISPATCH_TIME_FOREVER);
	dispatch_release(mySem);
	dispatch_release(myQueue);

	if (releaseTheGroup)
	{
		CFRelease(theGroup);
		theGroup = NULL;
	}
	CFRelease(collectTransform);

	return myResult;
}

void SecTransformExecuteAsync(SecTransformRef transformRef,
							  dispatch_queue_t deliveryQueue,
							  SecMessageBlock deliveryBlock)
{
	CFErrorRef localError = NULL;
	InternalSecTransformExecute(transformRef, &localError, deliveryQueue, deliveryBlock);

	// if we got an error (usually a transform startup error), we must deliver it
	if (localError != NULL)
	{
		// The monitor treats a NULL queue as running on it's own queue, which from an appication's point of view is
		// the same as a default global queue.   Chances are there is no monitor at this point, so it is best to just use the
		//  global queue for this.
		dispatch_queue_t effectiveQueue = deliveryQueue ? deliveryQueue : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, NULL);
		dispatch_async(effectiveQueue, ^{
			deliveryBlock(NULL, localError, true);
		});
	}
}

CFDictionaryRef SecTransformCopyExternalRepresentation(SecTransformRef transformRef)
{

	Transform* tr = (Transform*) CoreFoundationHolder::ObjectFromCFType(transformRef);
	return tr->Externalize(NULL);
}

CFStringRef SecTransformDotForDebugging(SecTransformRef transformRef)
{
    if (CFGetTypeID(transformRef) == SecGroupTransformGetTypeID()) {
        GroupTransform* tr = (GroupTransform*) CoreFoundationHolder::ObjectFromCFType(transformRef);
        return tr->DotForDebugging();
    } else {
        return CFSTR("Can only dot debug a group");
    }
}

SecTransformRef SecTransformCreateFromExternalRepresentation(
								CFDictionaryRef dictionary,
								CFErrorRef *error)
{
	// The incoming dictionary consists of a list of transforms and
	// a list of connections.  We start by making the individual
	// transforms and storing them in a dictionary so that we can
	// efficiently make connections

	CFArrayRef transforms = (CFArrayRef) CFDictionaryGetValue(dictionary, EXTERN_TRANSFORM_TRANSFORM_ARRAY);
	if (transforms == NULL)
	{
		// The dictionary we got is massively malformed!
		*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInputDictionary, "%@ is missing from the dictionary.  The dictionary is malformed.", EXTERN_TRANSFORM_TRANSFORM_ARRAY);
		return NULL;
	}

	CFArrayRef connections = (CFArrayRef) CFDictionaryGetValue(dictionary, EXTERN_TRANSFORM_CONNECTION_ARRAY);
	if (connections == NULL)
	{
		// The dictionary we got is massively malformed!
		*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInputDictionary, "%@ is missing from the dictionary.  The dictionary is malformed.", EXTERN_TRANSFORM_CONNECTION_ARRAY);
		return NULL;
	}

	CFMutableDictionaryRef transformHolder = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFTypeRefHolder _(transformHolder);

	CFIndex numTransforms = CFArrayGetCount(transforms);
	CFIndex n;

	SecTransformRef aTransform;

	for (n = 0; n < numTransforms; ++n)
	{
		// get the basic info we need
		CFDictionaryRef xTransform = (CFDictionaryRef) CFArrayGetValueAtIndex(transforms, n);

		CFStringRef xName = (CFStringRef) CFDictionaryGetValue(xTransform, EXTERN_TRANSFORM_NAME);

		CFStringRef xType = (CFStringRef) CFDictionaryGetValue(xTransform, EXTERN_TRANSFORM_TYPE);

		// reconstruct the transform
		aTransform = TransformFactory::MakeTransformWithType(xType, error);
		SecTransformSetAttribute(aTransform, kSecTransformTransformName, xName, NULL);

		// restore the transform state
		Transform* tr = (Transform*) CoreFoundationHolder::ObjectFromCFType(aTransform);
		tr->RestoreState((CFDictionaryRef) CFDictionaryGetValue(xTransform, EXTERN_TRANSFORM_STATE));
		tr->SetCustomExternalData((CFDictionaryRef) CFDictionaryGetValue(xTransform, EXTERN_TRANSFORM_CUSTOM_EXPORTS_DICTIONARY));

		CFIndex cnt = CFDictionaryGetCount(transformHolder);

		// add the transform to the dictionary
		CFDictionaryAddValue(transformHolder, xName, aTransform);

		if (CFDictionaryGetCount(transformHolder) <= cnt)
		{
			if (error)
			{
				*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInputDictionary,
							"Out of memory, or damaged input dictonary (duplicate label %@?)", xName);
			}
			return NULL;
		}
	}

	CFIndex numConnections = CFArrayGetCount(connections);
	if (numConnections == 0)
	{
		return aTransform;
	}

	SecGroupTransformRef gt = SecTransformCreateGroupTransform();

	for (n = 0; n < numConnections; ++n)
	{
		CFDictionaryRef connection = (CFDictionaryRef) CFArrayGetValueAtIndex(connections, n);
		CFStringRef fromTransformName = (CFStringRef) CFDictionaryGetValue(connection, EXTERN_TRANSFORM_FROM_NAME);
		CFStringRef fromAttribute = (CFStringRef) CFDictionaryGetValue(connection, EXTERN_TRANSFORM_FROM_ATTRIBUTE);
		CFStringRef toTransformName = (CFStringRef) CFDictionaryGetValue(connection, EXTERN_TRANSFORM_TO_NAME);
		CFStringRef toAttribute = (CFStringRef) CFDictionaryGetValue(connection, EXTERN_TRANSFORM_TO_ATTRIBUTE);

		SecTransformRef fromTransform = (SecTransformRef) CFDictionaryGetValue(transformHolder, fromTransformName);
		if (!fromTransform) {
			if (error) {
				*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInputDictionary, "Can't connect %@ to %@ because %@ was not found", fromTransformName, toTransformName, fromTransformName);
			}
			return NULL;
		}
		SecTransformRef toTransform = (SecTransformRef) CFDictionaryGetValue(transformHolder, toTransformName);
		if (!toTransform) {
			if (error) {
				*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInputDictionary, "Can't connect %@ to %@ because %@ was not found", fromTransformName, toTransformName, toTransformName);
			}
			return NULL;
		}

		aTransform = SecTransformConnectTransforms(fromTransform, fromAttribute, toTransform, toAttribute, gt, error);
	}

	return gt;
}



SecTransformRef SecTransformFindByName(SecTransformRef transform, CFStringRef name)
{
	Transform *t = (Transform*)CoreFoundationHolder::ObjectFromCFType(transform);
    GroupTransform *g = t->GetRootGroup();

    if (g) {
        return g->FindByName(name);
    } else {
        // There is no group, so if transform isn't our guy nobody is.
        return (CFStringCompare(name, t->GetName(), 0) == kCFCompareEqualTo) ? transform : NULL;
    }
}



CFTypeID SecGroupTransformGetTypeID()
{
	return GroupTransform::GetCFTypeID();
}

CFTypeID SecTransformGetTypeID()
{
	// Obviously this is wrong (returns same CFTypeID as SecTransformGetTypeID) Needs to be fixed
	return GroupTransform::GetCFTypeID();
}
