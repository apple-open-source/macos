/*
 *  SecCustomTransform.cpp
 *  libsecurity_transform
 *
 *  Created by JOsborne on 2/18/10.
 *  Copyright 2010 Apple. All rights reserved.
 *
 */

#include "SecCustomTransform.h"

#include "TransformFactory.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Block.h>
#include <syslog.h>
#include "Utilities.h"
#include "misc.h"

static const CFStringRef kSecCustom = CFSTR("CustomTransform");
static const char *kSecCustom_cstr = "CustomTransform";
const CFStringRef kSecTransformPreviousErrorKey = CFSTR("PreviousError");
const CFStringRef kSecTransformAbortOriginatorKey = CFSTR("Originating Transform");
const CFStringRef kSecTransformActionCanExecute = CFSTR("CanExecute");
const CFStringRef kSecCustomTransformWhatIsRequired = CFSTR("WhatIsRequired");
const CFStringRef kSecCustomTransformAttributesToExternalize = CFSTR("AttributesToExternalize");
const CFStringRef kSecTransformActionStartingExecution = CFSTR("ExecuteStarting");
const CFStringRef kSecTransformActionProcessData = CFSTR("TransformProcessData");
const CFStringRef kSecTransformActionAttributeNotification = CFSTR("GenericAttributeSetNotification");
const CFStringRef kSecTransformActionFinalize = CFSTR("Finalize");
const CFStringRef kSecTransformActionExternalizeExtraData = CFSTR("ExternalizeExtraData");
const CFStringRef kSecTransformActionInternalizeExtraData = CFSTR("InternalizeExtraData");
const CFStringRef kSecTransformActionAttributeValidation = CFSTR("AttributeValidation");

/*!
	@function		SecTransformOverrideTransformAction
	
	@abstract		Used to override the default behavior of a custom transform. 
	
	@param action	This should be either kSecTransformActionCanExecute, 
					kSecTransformActionStartingExecution, or 
					kSecTransformActionFinalize which signifies the behavior
					that is being overridden.									
					
	@param newAction	
					A SecTransformAttributeActionBlock block that implements the 
					override behavior.  Please see the 
					SecTransformActionBlock discussion for more 
					information.
				
	@result			A CFErrorRef if an error occurred, NULL otherwise.
					
	@discussion		An action may be overridden more then once, the most
					recent override will be used.Please see the example in 
					the documentation for the SecTransformActionBlock 
					block.
					
*/
typedef CFTypeRef (^SecTransformOverrideTransformAction)(CFStringRef action, 
								SecTransformActionBlock newAction);
								
/*!
	@function		SecTransformOverrideDataAction
	
	@abstract		Changes the default attribute handling for a 
					specified attribute.
					
	@param action	This should be either kSecTransformActionProcessData, 
					kSecTransformActionExternalizeExtraData which signifies  
					what behavior is being overridden.
															
	@param newAction	 
					A SecTransformDataBlock block that implements the 
					override behavior. Please see the 
					SecTransformDataBlock discussion for more 
					information.

	@result			A CFErrorRef if an error occurred. NULL otherwise.
					
	@discussion		An action may be overridden more then once, the most
				    recent override will be used. Please see the example 
					in the documentation for the SecTransformAttributeActionBlock 
					block.
				
*/
typedef CFTypeRef (^SecTransformOverrideDataAction)(CFStringRef action, 
								SecTransformDataBlock newAction);
								
/*!
	@function		SecTransformOverrideAttributeAction
	
	@abstract		Changes the default attribute handling for a 
					specified attribute.
					
	@param action	This should be either SecTransformSetAttributeAction, 
					kSecTransformActionAttributeValidation which signifies  
					what behavior is being overridden.
	
	@param attribute	
					The attribute whose attribute default attribute handling is 
					being overridden.  Passing NULL will override all attributes
					that have not been specifically overridden.
										
	@param newAction	 
					A SecTransformAttributeActionBlock block 
					that implements the  override behavior. 
					
					If the action parameter is SecTransformSetAttributeAction
					then this block is called whenever a set is called on the
					attribute that this block was registered for or in the case
					of a NULL attribute name any attribute that has not been specifically
					overridden.  The block may transmogrify the data as needed.  It may
					also send the data to any other attribue by calling 
					SecTransformCustomSetAttribute.  The value returned from the block 
					will be the new value for the attribute.
					
					If the action parameter is kSecTransformActionAttributeValidation then
					this block is called to validate the new value for the
					attribute that this block was registered for or in the case
					of a NULL attribute name any attribute that has not been specifically
					overridden.  The block should test if the new value is acceptable
					and return NULL if it is valid a CFErrorRef otherwise.
					
	@result			A CFErrorRef if an error occurred. NULL otherwise.
					
	@discussion		An action may be overridden more then once, the most
				    recent override will be used. Please see the example 
					in the documentation for the 
					SecTransformAttributeActionBlock block.
				
*/
typedef CFTypeRef (^SecTransformOverrideAttributeAction)( 
								CFStringRef action,
								SecTransformStringOrAttributeRef attribute, 
								SecTransformAttributeActionBlock newAction);

								
/*!
	@function		SecTransformGetAttributeBlock
	
	@abstract		Retrieves the value of the attribute metadata of the 
					type specified.
					
	@param attribute	
					The attribute from which to retrieve the metadata from.
					
	@param type		The type of the metadata to be fetched.
						
	@result			The value of the metadata that was retrieved or a CFErrorRef
					if an error occurred
	
	@result 		The value of the metadata that was retrieved.
	

*/
typedef CFTypeRef (^SecTransformGetAttributeBlock)(
										SecTransformStringOrAttributeRef attribute, 
										SecTransformMetaAttributeType type);
										
/*!
	@function		SecTransformSetAttributeBlock
	
	@abstract		This sets the value of the metadata of an attribute.  
		
	@param attribute	
					The attribute whose value is sent
					
	@param type		The metadata type that specifies what metadata value
					is set.
					
	@param value	The value of the metadata to be sent.
						
	@result			A CFErrorRef is an error occurred, NULL otherwise.
	
	@discussion		The attribute parameter specifies which attribute will 
					have its data set. The type parameter specifies which of 
					the metadata items is set. The value parameter is the 
					new metadata value.
							
*/
typedef CFErrorRef (^SecTransformSetAttributeBlock)(
	SecTransformStringOrAttributeRef attribute, 
	SecTransformMetaAttributeType type, 
	CFTypeRef value);
	

/*!
	@function		SecTransformPushBackAttributeBlock

	@abstract		Allows for putting a single value back for a 
					specific attribute.  This will stop the flow of
					data into the specified attribute until an
					attribute is changed.

	@param attribute	
					The attribute that has its data pushed back.

	@param value	The value being pushed back.


	@result			A CFErrorRef is an error occurred, NULL otherwise.
					Note: pushing back a second value will abort the
					transform, not return an error from this call.

	@discussion		A particular custom transform may need multple
					values to be set before it can do the processing
					that the custom transform is designed to do. For
					example, it may need a key and a salt value.  The
					salt value maybe supplied by another transform while
					the key transform may have been set explicitly.  When
					data is presented to this custom transform the salt
					value may not have been sent from the upstream transform.
					The custom transform can then push back the input data
					which causes the transform to stall.  When any 
					attribute on the custom transform is changed, such as 
					the upstream transform delivers the salt value, then 
					the data that was pushed back is re-delivered

*/
typedef CFErrorRef (^SecTransformPushBackAttributeBlock)(
										SecTransformStringOrAttributeRef attribute, 
										CFTypeRef value);

/*!
	@const kSecTransformCreateBlockParametersVersion
			The current version number of the SecTransformCreateBlockParameters
			struct
	
*/
enum 
{
	kSecTransformCreateBlockParametersVersion = 1
};

extern "C" {
	Boolean SecExternalSourceSetValue(SecTransformRef xst, CFTypeRef value, CFErrorRef *error);
}

/*!
	@struct OpaqueSecTransformImplementation
	
	@field version
			The version number of this structure
			
	@field overrideTransform
			A SecTransformOverrideTransformAction block.  See
			the headerdoc for this block for additional information.
			
	@field overrideAttribute
			A SecTransformOverrideAttributeAction block. See
			the headerdoc for this block for additional information.

	@field get
			A SecTransformGetAttributeBlock block. See
			the headerdoc for this block for additional information.
			
	@field send
		A SecTransformSetAttributeBlock block.  See
		the headerdoc for this block for additional information.
		
	@field pushback
		A SecTransformPushBackAttributeBlock block. See
		the headerdoc for this block for additional information.
*/
struct OpaqueSecTransformImplementation
{
	CFIndex version;	// Set to kSecTransformCreateBlockParametersVersion
	
	// The following two blocks allow for overriding 'standard' 
	// transform behavior
	SecTransformOverrideTransformAction overrideTransform;
	SecTransformOverrideDataAction overrideData;
	SecTransformOverrideAttributeAction overrideAttribute;
	
	// The following methods allow for dealing with the transform mechanism
	// They are called synchronously
	SecTransformGetAttributeBlock get;
	SecTransformSetAttributeBlock send;
	SecTransformPushBackAttributeBlock pushback;
};

										
class CustomTransformFactory : public TransformFactory 
{
protected:
	SecTransformCreateFP  createFuncPtr;
public:
	CustomTransformFactory(CFStringRef name, SecTransformCreateFP createFP, CFErrorRef *error);
	virtual CFTypeRef Make();
};


static SecTransformActionBlock default_can_run = ^{ return (CFTypeRef)NULL; };
static SecTransformActionBlock default_execute_starting = default_can_run;
static SecTransformActionBlock default_finalize = default_execute_starting;
static SecTransformActionBlock default_externalize_data = default_finalize;

static SecTransformDataBlock default_process_data = ^(CFTypeRef value) { return value; };
//static SecTransformDataBlock default_validate = ^(CFTypeRef value) { return (CFTypeRef)NULL; };
static SecTransformAttributeActionBlock default_generic_attribute_set_notification = 
	^(SecTransformAttributeRef ah, CFTypeRef value) { return value; };

static SecTransformAttributeActionBlock default_generic_attribute_validation = 
^(SecTransformAttributeRef ah, CFTypeRef value) 
{
	 return (CFTypeRef)NULL;
};

static SecTransformDataBlock default_internalize_data = 
			^(CFTypeRef value) 
{ 
	return (CFTypeRef)NULL; 
};

class CustomTransform : public Transform 
{
protected:
	SecTransformCreateFP					createFuncPtr;
	SecTransformInstanceBlock			instanceBlock;	  
			
	SecTransformActionBlock 			can_run;
	SecTransformActionBlock 			execute_starting;
	SecTransformActionBlock 			finalize;
	SecTransformAttributeActionBlock 	generic_attribute_set_notification;
	SecTransformAttributeActionBlock	generic_attribute_validation;
	SecTransformDataBlock 				process_data;
	SecTransformActionBlock 			externalize_data;
	SecTransformDataBlock 				internalize_data;
		
	SecTransformRef tr;	
	
	SecTransformAttributeRef 			input_ah;
	SecTransformAttributeRef			output_ah;
	
	OpaqueSecTransformImplementation parameters;
	
	void SetCanExecute(SecTransformActionBlock CanExecuteBlock)
	{
		Block_release(can_run);
		if (CanExecuteBlock)
		{
			can_run = Block_copy(CanExecuteBlock);
			
		}
		else
		{
			can_run	 = Block_copy(default_can_run);
		}
	}
	
	void SetExecuteStarting(SecTransformActionBlock executeStartingBlock)
	{
		Block_release(execute_starting);
		if (executeStartingBlock)
		{
			execute_starting = Block_copy(executeStartingBlock);
			
		}
		else
		{
			execute_starting = Block_copy(default_execute_starting);
		}
	}
	
	void SetFinalize(SecTransformActionBlock finalizeBlock)
	{
		Block_release(finalize);
		if (finalizeBlock)
		{
			finalize = Block_copy(finalizeBlock);
			
		}
		else
		{
			finalize = Block_copy(default_finalize);
		}
	}
	
	void SetExternalizeExtraData(SecTransformActionBlock externalizeBlock)
	{
		Block_release(externalizeBlock);
		if (externalizeBlock)
		{
			externalize_data = Block_copy(externalizeBlock);
			
		}
		else
		{
			externalize_data = Block_copy(default_externalize_data);
		}
	}
	
	void SetProcessData(SecTransformDataBlock processDataBlock)
	{
		Block_release(process_data);
		if (processDataBlock)
		{
			process_data = Block_copy(processDataBlock);
			
		}
		else
		{
			process_data = Block_copy(default_process_data);
		}
	}
	
	void SetInternalizeExtraData(SecTransformDataBlock InternalizeExtraDataBlock)
	{
		Block_release(internalize_data);
		if (InternalizeExtraDataBlock)
		{
			internalize_data = Block_copy(InternalizeExtraDataBlock);
			
		}
		else
		{
			internalize_data = Block_copy(default_internalize_data);
		}
	}
	
	
		
	void SetNotficationBlock(SecTransformStringOrAttributeRef attribute, 
							SecTransformAttributeActionBlock notificationBlock)
	{
		SecTransformAttributeActionBlock blockToSet = 
			Block_copy((notificationBlock) ? notificationBlock : 
				   default_generic_attribute_set_notification);
		
		if (attribute)
		{
			transform_attribute *ta = getTA(attribute, true);
			
			if (ta->attribute_changed_block) 
			{
				Block_release(ta->attribute_changed_block);
			}
			
			ta->attribute_changed_block = blockToSet;
		}
		else		
		{	
			
			if (generic_attribute_set_notification) 
			{
				Block_release(generic_attribute_set_notification);
			}
			
			generic_attribute_set_notification = blockToSet;			
		}
	}

	void SetVerifyBlock(SecTransformStringOrAttributeRef attribute, 
							SecTransformAttributeActionBlock verifyBlock)
	{
		SecTransformAttributeActionBlock blockToSet = 
			Block_copy((verifyBlock) ? verifyBlock : 
				   generic_attribute_validation);
		
		if (attribute)
		{
			transform_attribute *ta = getTA(attribute, true);
			
			if (ta->attribute_validate_block) 
			{
				Block_release(ta->attribute_validate_block);
			}
			
			ta->attribute_validate_block = blockToSet;
		}
		else		
		{	
			if (generic_attribute_validation) 
			{
				Block_release(generic_attribute_validation);
			}
			
			generic_attribute_validation = blockToSet;			
		}
	}
		
		

public:
	CustomTransform(CFStringRef name, SecTransformCreateFP createFP);
	virtual ~CustomTransform();
	
	void Create();
	
	CFTypeRef rebind_data_action(CFStringRef action, 
		SecTransformDataBlock new_action);
		
	CFTypeRef rebind_transform_action(CFStringRef action, SecTransformActionBlock new_action);
	
	CFTypeRef rebind_attribute_action(CFStringRef action,
							SecTransformStringOrAttributeRef attribute, 
							SecTransformAttributeActionBlock new_action);
	
	SecTransformRef get_ref() { return tr; }
	
	virtual void AttributeChanged(CFStringRef name, CFTypeRef value);
	virtual void AttributeChanged(SecTransformAttributeRef ah, CFTypeRef value);
	virtual CFErrorRef TransformStartingExecution();
	virtual CFDictionaryRef GetCustomExternalData();
	virtual void SetCustomExternalData(CFDictionaryRef customData);
	
	friend Boolean SecExternalSourceSetValue(SecTransformRef xst, CFTypeRef value, CFErrorRef *error);
};



#pragma mark CustomTransformFactory

CustomTransformFactory::CustomTransformFactory(CFStringRef uniqueName, SecTransformCreateFP createFP, CFErrorRef* error) : 
	TransformFactory(uniqueName, false, kSecCustom), 
	createFuncPtr(createFP) 
{
	TransformFactory *existing = FindTransformFactoryByType(uniqueName);
	if (existing) 
	{
		if (error) 
		{
			*error = CreateSecTransformErrorRef(kSecTransformErrorNameAlreadyRegistered, 
				"Custom transform type %s already exists.", uniqueName);
		}
        return;
	}
	
    
    if (CFStringGetCharacterAtIndex(uniqueName, 0) == '_')
    {
        if (error)
        {
            *error = CreateSecTransformErrorRef(kSecTransformInvalidArgument, 
                                                "Invalid transform type name %s -- type names must not start with an _", uniqueName);
        }
        return;
    }
    
    static CFCharacterSetRef invalidTypeCharactors = NULL;
    static dispatch_once_t setupInvalidTypeCharactors;
    dispatch_once(&setupInvalidTypeCharactors, ^{
        invalidTypeCharactors = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("/:"));
    });
    CFRange has_bad;
    if (CFStringFindCharacterFromSet(uniqueName, invalidTypeCharactors, CFRangeMake(0, CFStringGetLength(uniqueName)), 0, &has_bad)) {
        if (error)
        {
            *error = CreateSecTransformErrorRef(kSecTransformInvalidArgument, 
                                            "Invalid character '%c' in transform type name %s", CFStringGetCharacterAtIndex(uniqueName, has_bad.location), uniqueName);
        }
        return;
    }
    RegisterTransform(this, kSecCustom);
}

CFTypeRef CustomTransformFactory::Make() 
{
	CustomTransform *ct = new CustomTransform(this->GetTypename(), createFuncPtr);
	ct->Create();
	return ct->get_ref();
}

#pragma mark MISC

const void *Block_copy_a(CFAllocatorRef allocator, const void *block) {
	return Block_copy(block);
}

void Block_release_a(CFAllocatorRef allocator, const void *block) {
	Block_release(block);
}

extern "C" {
	SecTransformAttributeActionBlock SecTransformCreateValidatorForCFtype(CFTypeID expected_type, Boolean null_allowed) {
		SecTransformAttributeActionBlock validate = NULL;
		CFErrorRef (^make_error_message)(SecTransformAttributeRef attr, CFTypeRef value, CFTypeID expected_type, Boolean null_allowed) = 
		^(SecTransformAttributeRef attr, CFTypeRef value, CFTypeID expected_type, Boolean null_allowed) {
			CFStringRef expected_type_name = CFCopyTypeIDDescription(expected_type);
			CFErrorRef error = NULL;
			if (value) {
				CFStringRef value_type_name = CFCopyTypeIDDescription(CFGetTypeID(value));
				error = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "%@ received value of type %@ (%@), expected%@ a %@%@",
															  attr, value_type_name, value,
															  null_allowed ? CFSTR(" either") : CFSTR(""),
															  expected_type_name,
															  null_allowed ? CFSTR(" or a NULL") : CFSTR(""));
				CFRelease(value_type_name);
			} else {
				error = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "%@ received NULL value, expected a %@",
												   attr, expected_type_name);
			}
			CFRelease(expected_type_name);
			
			return error;
		};
		
		
		if (null_allowed) {
			validate = ^(SecTransformAttributeRef attr, CFTypeRef value) {
				if (value == NULL || CFGetTypeID(value) == expected_type) {
					return (CFTypeRef)NULL;
				}
				return (CFTypeRef)make_error_message(attr, value, expected_type, null_allowed);
			};
		} else {
			validate = ^(SecTransformAttributeRef attr, CFTypeRef value) {
				if (value != NULL && CFGetTypeID(value) == expected_type) {
					return (CFTypeRef)NULL;
				}
				return (CFTypeRef)make_error_message(attr, value, expected_type, null_allowed);
			};
		}
		
		return Block_copy(validate);
	}
}

Boolean SecTransformRegister(CFStringRef uniqueName, SecTransformCreateFP createFP, CFErrorRef *caller_error) 
{
	CFErrorRef error = NULL;
	
	CustomTransformFactory *tf = new CustomTransformFactory(uniqueName, createFP, &error);
	if (error) 
	{
		delete tf;
		if (caller_error) 
		{
			*caller_error = error;
		}
		return FALSE;
	} 
	else 
	{
		return TRUE;
	}
}

SecTransformRef SecTransformCreate(CFStringRef name, CFErrorRef *error) 
{
	SecTransformRef tr = TransformFactory::MakeTransformWithType(name, error);
	return tr;
}

extern "C" {
	Boolean SecExternalSourceSetValue(SecTransformRef xst, CFTypeRef value, CFErrorRef *error)
	{
		CustomTransform *ct = (CustomTransform *)CoreFoundationHolder::ObjectFromCFType(xst);
		extern CFStringRef external_source_name;
		if (CFEqual(ct->mTypeName, external_source_name)) {
			ct->SetAttribute(ct->input_ah, value);
			return true;
		} else {
			if (error) {
				*error = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "SecExternalSourceSetValue called for %@, you need to pass in an ExternalSource transform not a %@", ct->GetName(), ct->mTypeName);
			}
			return false;
		}
	}
}

/* ==========================================================================
	class:			NoDataClass
	description:	A Special CFType that signifies that no data is being 
					returned
   ==========================================================================*/
#pragma mark NoDataClass

class NoDataClass : public CoreFoundationObject
{
protected:
	NoDataClass();

public:
	virtual ~NoDataClass();
	std::string FormattingDescription(CFDictionaryRef options);
	std::string DebugDescription();
	static CFTypeRef Make();
};

CFTypeRef NoDataClass::Make() {
	NoDataClass* obj = new NoDataClass();
	return CoreFoundationHolder::MakeHolder(gInternalProtectedCFObjectName, obj);
}


NoDataClass::NoDataClass() : CoreFoundationObject(gInternalProtectedCFObjectName) {
}

NoDataClass::~NoDataClass()
{
}

std::string NoDataClass::DebugDescription() 
{
	return CoreFoundationObject::DebugDescription() + " | SecTransformNoData";
}

std::string NoDataClass::FormattingDescription(CFDictionaryRef options) 
{
	return CoreFoundationObject::FormattingDescription(options) + " | SecTransformNoData";
}

CFTypeRef SecTransformNoData() 
{
	static dispatch_once_t inited;
	static CFTypeRef no_data;
	
	dispatch_once(&inited, 
	^{
		no_data = NoDataClass::Make();
	});
	
	return no_data;
}

/* ==========================================================================
	class Implementation	CustomTransform	
   ==========================================================================*/

#pragma mark CustomTransform

void CustomTransform::AttributeChanged(CFStringRef name, CFTypeRef value) {
#ifndef NDEBUG
	// We really shouldn't get here, and this is the debug build so we can blow up on the spot so it is easy to look at the stack trace
	abort();
#else
	// We really shouldn't get here, but this is a production build and recovery is easy to code (but costly to execute)
	AttributeChanged(getAH(name, false), value);
#endif
}

void CustomTransform::AttributeChanged(SecTransformAttributeRef ah, CFTypeRef value) 
{
	transform_attribute *ta = ah2ta(ah);
	SecTransformAttributeActionBlock attribute_set_notification = NULL;

	SecTransformAttributeActionBlock attribute_validate = NULL;

	attribute_validate = (SecTransformAttributeActionBlock)ta->attribute_validate_block;
	if (!attribute_validate) {
			attribute_validate = generic_attribute_validation;
	}
	CFTypeRef vr = attribute_validate(ah, value);
	if (vr) {
			if (CFGetTypeID(vr) == CFErrorGetTypeID()) {
					SendAttribute(AbortAH, vr);
					CFRelease(vr);
			} else {
					CFErrorRef e = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "Invalid return type from a validate action, expected a CFErrorRef got a %@ (%@)", CFCopyTypeIDDescription(CFGetTypeID(vr)), vr);
					SendAttribute(AbortAH, e);
					CFRelease(vr);
					// XXX: this causes a core dump -- I think AbortAH doesn't take it's own reference!!   CFRelease(e);
			}
			return;
	}

	attribute_set_notification = (SecTransformAttributeActionBlock)ta->attribute_changed_block;

	if ((!attribute_set_notification) && ah == input_ah) 
	{
		CFTypeID vtype = value ? CFGetTypeID(value) : CFDataGetTypeID();
		if (vtype == CFDataGetTypeID()) 
		{
			CFTypeRef output = process_data(value);
			if (output == NULL || output != SecTransformNoData()) 
			{
				SendAttribute(output_ah, output);

                // if output == value, we are being asked to just
                // forward the existing value.  No need to release.
                // If they are different, we are being asked to
                // send a new value which must be released.

                if (output != value && output != NULL)
                {
                    CFRelease(output);
                }
			}
		} 
		else if (vtype == CFErrorGetTypeID() && !ah2ta(ah)->direct_error_handling) 
		{
			SendAttribute(output_ah, value);
		} else 
		{
			attribute_set_notification = attribute_set_notification ? attribute_set_notification : generic_attribute_set_notification;
			CFTypeRef new_value = attribute_set_notification(ah, value);
			if (new_value != value) 
			{
				SendAttribute(ah, new_value);
			}
		}
	} 
	else 
	{
		CFTypeID vtype = value ? CFGetTypeID(value) : CFDataGetTypeID();
		if (vtype != CFErrorGetTypeID() || ah2ta(ah)->direct_error_handling) 
		{
			attribute_set_notification = attribute_set_notification ? attribute_set_notification : generic_attribute_set_notification;
			CFTypeRef new_value = attribute_set_notification(ah, value);
			if (new_value != value) 
			{
				SendAttribute(ah, new_value);
			}
		} 
		else 
		{
			SendAttribute(output_ah, value);
		}
	}
}

CFTypeRef CustomTransform::rebind_data_action(CFStringRef action, 
			SecTransformDataBlock new_action) 
{
	CFTypeRef result = NULL;
	if (kCFCompareEqualTo == CFStringCompare(kSecTransformActionProcessData, action, 0)) 
	{
		SetProcessData(new_action);
	}
	else if (kCFCompareEqualTo == CFStringCompare(kSecTransformActionInternalizeExtraData, action, 0))
	{
		SetInternalizeExtraData(new_action);
	}
	else 
	{
		result = (CFTypeRef)CreateSecTransformErrorRef(kSecTransformInvalidOverride, "Unkown override type");

        // XXX: can we get a stackdump here too?
        CFStringRef msg = CFStringCreateWithFormat(NULL, NULL,
                                                   CFSTR("rebind_data_action (action %@, new_action %p, transform %s)"),
                                                   action, (void*)new_action, DebugDescription().c_str());
        char *utf8_message = utf8(msg);
        syslog(LOG_ERR, "%s", utf8_message);
        free(utf8_message);
        CFRelease(msg);
	}
	return result;
}

CFTypeRef CustomTransform::rebind_transform_action(CFStringRef action, SecTransformActionBlock new_action) 
{
	CFErrorRef result = NULL;
	
	if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionCanExecute, 0)) 
	{
		SetCanExecute(new_action);
	}
	else if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionStartingExecution, 0)) 
	{
		SetExecuteStarting(new_action);
	}
	else if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionFinalize, 0))
	{
		SetFinalize(new_action);
	}
	else if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionExternalizeExtraData, 0))
	{
		SetExternalizeExtraData(new_action);
	}
	else
	{
		result = CreateSecTransformErrorRef(kSecTransformInvalidOverride, "Unkown override type");

		char *action_utf8 = utf8(action);
		syslog(LOG_ERR, "rebind_transform_action (action %s, all-attributes, block %p, transform %s)\n", action_utf8, (void*)new_action, DebugDescription().c_str());
		free(action_utf8);
	}
		
	return result;
}

CFTypeRef CustomTransform::rebind_attribute_action(
			CFStringRef action,
			SecTransformStringOrAttributeRef attribute, 
			SecTransformAttributeActionBlock new_action) 
{
	CFErrorRef result = NULL;
	
	if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionAttributeNotification, 0)) 
	{
		SetNotficationBlock(attribute, new_action);
	}
	else if (kCFCompareEqualTo == CFStringCompare(action, kSecTransformActionAttributeValidation, 0))
	{
		SetVerifyBlock(attribute, new_action);
	}
	else
	{
		result = CreateSecTransformErrorRef(kSecTransformInvalidOverride, "Unkown override type");
		char *action_utf8 = utf8(action);
		syslog(LOG_ERR, "rebind_attribute_action (action %s, all-attributes, block %p, transform %s)\n", action_utf8, (void*)new_action, DebugDescription().c_str());
		free(action_utf8);
	}
	
	return result;	
}

CustomTransform::CustomTransform(CFStringRef cfname, SecTransformCreateFP createFP) : 
	Transform(cfname), 
	createFuncPtr(createFP), 
	instanceBlock(NULL),
	can_run(Block_copy(default_can_run)), 
	execute_starting(Block_copy(default_execute_starting)), 
	finalize(Block_copy(default_finalize)), 
	generic_attribute_set_notification(Block_copy(default_generic_attribute_set_notification)),
	generic_attribute_validation(Block_copy(default_generic_attribute_validation)),
	process_data(Block_copy(default_process_data)),
	externalize_data(Block_copy(default_externalize_data)),
	internalize_data(Block_copy(default_internalize_data))
{
	mAlwaysSelfNotify = true;
	
	input_ah = getAH(kSecTransformInputAttributeName, true);
	output_ah = getAH(kSecTransformOutputAttributeName, true);
	
	parameters.version = kSecTransformCreateBlockParametersVersion;
	parameters.send = Block_copy(^(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type, CFTypeRef value) 
	{ 
		return SendMetaAttribute(attribute, type, value); 
	});
	
	parameters.pushback = Block_copy(^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
	{ 
		return Pushback(getAH(attribute), value); 
	});
	
	parameters.get = Block_copy(^(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type) 
	{ 
		return GetMetaAttribute(attribute, type); 
	});
	
	parameters.overrideTransform = Block_copy(^(CFStringRef action, SecTransformActionBlock new_action) 
	{ 
		return rebind_transform_action(action, new_action); 
	});
	
	parameters.overrideData = Block_copy(^(CFStringRef action, 
								SecTransformDataBlock new_action)
	{
		return rebind_data_action(action, new_action); 
	});
	
	/*
	 CFTypeRef (^SecTransformOverrideAttributeAction)( 
	 CFStringRef action,
	 SecTransformStringOrAttributeRef attribute, 
	 SecTransformAttributeActionBlock newAction);
	*/
	parameters.overrideAttribute = 
		Block_copy(^(CFStringRef action, SecTransformStringOrAttributeRef attribute, SecTransformAttributeActionBlock new_action) 
	{ 
		return rebind_attribute_action(action, attribute, new_action); 
	});
	
	char *tname = const_cast<char*>(CFStringGetCStringPtr(cfname, kCFStringEncodingUTF8));
	if (!tname) {
		CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfname), kCFStringEncodingUTF8);
		tname = static_cast<typeof(tname)>(alloca(sz));
		CFStringGetCString(cfname, tname, sz, kCFStringEncodingUTF8);
	}	
	tr = CoreFoundationHolder::MakeHolder(kSecCustom, (CoreFoundationObject*)this);
	
	instanceBlock = (*createFuncPtr)(cfname, tr, &parameters);
}

void CustomTransform::Create()
{
	(void)instanceBlock();
}


CustomTransform::~CustomTransform() {
	finalize();
	
	if (instanceBlock)
	{
		Block_release(instanceBlock);
	}
	
	Block_release(can_run);
	Block_release(execute_starting);
	Block_release(finalize);
	Block_release(generic_attribute_set_notification);
	Block_release(process_data);
	Block_release(externalize_data);
	Block_release(internalize_data);

	Block_release(parameters.send);
	Block_release(parameters.pushback);
	Block_release(parameters.get);
	Block_release(parameters.overrideTransform);
	Block_release(parameters.overrideData);
	Block_release(parameters.overrideAttribute);
	
	// strictly speaking this isn't needed, but it can help track down some "use after free" bugs
	tr = NULL;
	createFuncPtr = NULL;
	process_data = NULL;
}

CFErrorRef CustomTransform::TransformStartingExecution() 
{
	CFTypeRef result = execute_starting();
	return (CFErrorRef)result;
}


CFDictionaryRef CustomTransform::GetCustomExternalData()
{
	CFTypeRef result = externalize_data();
	if (NULL == result)
	{
		return NULL;
	}
	
	if (CFGetTypeID(result) == CFErrorGetTypeID())
	{
		// Ouch!  we should deal with this
		CFRelease(result);
		return NULL;
	}
	
	if (CFGetTypeID(result) == CFDictionaryGetTypeID())
	{
		return (CFDictionaryRef)result;
	}
	
	CFRelease(result);
	result = NULL;
	return (CFDictionaryRef)result;
}


void CustomTransform::SetCustomExternalData(CFDictionaryRef customData)
{
	if (NULL != customData)
	{
		internalize_data(customData);		
	}
	return;
}

CFErrorRef SecTransformSetAttributeAction(SecTransformImplementationRef ref, 
								CFStringRef action,
								SecTransformStringOrAttributeRef attribute, 
								SecTransformAttributeActionBlock newAction)								
{
	if (NULL == ref)
	{
		 CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
			"SecTransformSetAttributeNotificationAction called with a NULL SecTransformImplementationRef ref");
			
		return result;
	}
	
	return (CFErrorRef)ref->overrideAttribute(action, attribute, newAction);
}

CFErrorRef SecTransformSetDataAction(SecTransformImplementationRef ref, 
									CFStringRef action, 
									SecTransformDataBlock newAction)
{
	if (NULL == ref)
	{
		 CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
			"SecTransformSetAttributeNotificationAction called with a NULL SecTransformImplementationRef ref");
			
		return result;
	}
	
	return (CFErrorRef)ref->overrideData(action, newAction);	
}

CFErrorRef SecTransformSetTransformAction(SecTransformImplementationRef ref,
								CFStringRef action, 
								SecTransformActionBlock newAction)
{
	if (NULL == ref)
	{
		 CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
			"SecTransformSetAttributeNotificationAction called with a NULL SecTransformImplementationRef ref");
			
		return result;
	}
	
	return (CFErrorRef)ref->overrideTransform(action, newAction);
}

CFTypeRef SecTranformCustomGetAttribute(SecTransformImplementationRef ref,
                                        SecTransformStringOrAttributeRef attribute,
                                        SecTransformMetaAttributeType type)
{
	if (NULL == ref)
	{
        CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
                                                       "SecTransformCustomGetAttribute called with a NULL SecTransformImplementationRef ref");
        
		return result;
	}
	
	return (CFErrorRef)ref->get(attribute, type);
}

CFTypeRef SecTransformCustomSetAttribute(SecTransformImplementationRef ref,
									SecTransformStringOrAttributeRef attribute, 
									SecTransformMetaAttributeType type, 
									CFTypeRef value)
{
	if (NULL == ref)
	{
		 CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
			"SecTransformCustomSetAttribute called with a NULL SecTransformImplementationRef ref");
			
		return result;
	}
	
	return (CFErrorRef)ref->send(attribute, type, value);
	
}

CFTypeRef SecTransformPushbackAttribute(SecTransformImplementationRef ref,
								SecTransformStringOrAttributeRef attribute, 
								CFTypeRef value)
{
	if (NULL == ref)
	{
		 CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, 
			"SecTransformPushbackAttribute called with a NULL SecTransformImplementationRef ref");
			
		return (CFTypeRef)result;
	}
	
	return ref->pushback(attribute, value);
}