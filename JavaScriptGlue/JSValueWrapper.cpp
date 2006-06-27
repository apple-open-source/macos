//
// JSValueWrapper.cpp
//
#include "JSValueWrapper.h"
#include <pthread.h>

JSValueWrapper::JSValueWrapper(const Value& inValue)
    : fValue(inValue)
{ 
}

JSValueWrapper::~JSValueWrapper()
{ 
}

Value& JSValueWrapper::GetValue() 
{ 
	return fValue; 
}

/*
 * This is a slight hack. The JSGlue API has no concept of execution state.
 * However, execution state is an inherent part of JS, and JSCore requires it.
 * So, we keep a single execution state for the whole thread and supply it
 * where necessary.

 * The execution state holds two things: (1) exceptions; (2) the global object. 
 * JSGlue has no API for accessing exceptions, so we just discard them. As for
 * the global object, JSGlue includes no calls that depend on it. Its property
 * getters and setters are per-object; they don't walk up the enclosing scope. 
 * Functions called by JSObjectCallFunction may reference values in the enclosing 
 * scope, but they do so through an internally stored scope chain, so we don't 
 * need to supply the global scope.
 */      

pthread_key_t interpreterKey;
pthread_once_t interpreterKeyOnce = PTHREAD_ONCE_INIT;

static void destroyInterpreter(void* data) 
{
    delete static_cast<Interpreter*>(data);
}

static void initializeInterpreterKey()
{
    pthread_key_create(&interpreterKey, destroyInterpreter);
}

static ExecState* getThreadGlobalExecState()
{
    pthread_once(&interpreterKeyOnce, initializeInterpreterKey);
    Interpreter* interpreter = static_cast<Interpreter*>(pthread_getspecific(interpreterKey));
    if (!interpreter) {
        interpreter = new Interpreter();
        pthread_setspecific(interpreterKey, interpreter);
    }

    // Discard exceptions -- otherwise an exception would forestall JS 
    // evaluation throughout the thread
    interpreter->globalExec()->clearException();

    return interpreter->globalExec();
}

void JSValueWrapper::GetJSObectCallBacks(JSObjectCallBacks& callBacks)
{
	callBacks.dispose = (JSObjectDisposeProcPtr)JSValueWrapper::JSObjectDispose;
	callBacks.equal = (JSObjectEqualProcPtr)NULL;
	callBacks.copyPropertyNames = (JSObjectCopyPropertyNamesProcPtr)JSValueWrapper::JSObjectCopyPropertyNames;
	callBacks.copyCFValue = (JSObjectCopyCFValueProcPtr)JSValueWrapper::JSObjectCopyCFValue;
	callBacks.copyProperty = (JSObjectCopyPropertyProcPtr)JSValueWrapper::JSObjectCopyProperty;
	callBacks.setProperty = (JSObjectSetPropertyProcPtr)JSValueWrapper::JSObjectSetProperty;
	callBacks.callFunction = (JSObjectCallFunctionProcPtr)JSValueWrapper::JSObjectCallFunction;
}
			
void JSValueWrapper::JSObjectDispose(void* data)
{
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	delete ptr;
}
	

CFArrayRef JSValueWrapper::JSObjectCopyPropertyNames(void* data)
{
    InterpreterLock lock;

	CFMutableArrayRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
        ExecState* exec = getThreadGlobalExecState();
#if JAG_PINK_OR_LATER
		Object object = ptr->GetValue().toObject(exec);
		ReferenceList list = object.propList(exec, false);
		ReferenceListIterator iterator = list.begin();
#else
		Object object = ptr->GetValue().imp()->toObject(exec);
                List list = object.propList(exec, false);
		ListIterator iterator = list.begin();
#endif


		while (iterator != list.end()) {
#if JAG_PINK_OR_LATER
			Identifier name = iterator->getPropertyName(exec);
			CFStringRef nameStr = IdentifierToCFString(name);
#else
			UString name = iterator->getPropertyName(exec);
			CFStringRef nameStr = UStringToCFString(name);
#endif
			if (!result)
			{
				result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			if (result && nameStr)
			{
				CFArrayAppendValue(result, nameStr);
			}
			ReleaseCFType(nameStr);
			iterator++;
		}

	}
	return result;
}


JSObjectRef JSValueWrapper::JSObjectCopyProperty(void* data, CFStringRef propertyName)
{
    InterpreterLock lock;

	JSObjectRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = getThreadGlobalExecState();
#if JAG_PINK_OR_LATER
		Value propValue = ptr->GetValue().toObject(exec).get(exec, CFStringToIdentifier(propertyName));
#else
		Value propValue = ptr->GetValue().imp()->toObject(exec).get(exec, CFStringToUString(propertyName));
#endif
		JSValueWrapper* wrapperValue = new JSValueWrapper(propValue);

		JSObjectCallBacks callBacks;
		GetJSObectCallBacks(callBacks);
		result = JSObjectCreateInternal(wrapperValue, &callBacks, JSValueWrapper::JSObjectMark, kJSUserObjectDataTypeJSValueWrapper);

		if (!result)
		{
			delete wrapperValue;
		}
	}
	return result;
}

void JSValueWrapper::JSObjectSetProperty(void* data, CFStringRef propertyName, JSObjectRef jsValue)
{
    InterpreterLock lock;

	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = getThreadGlobalExecState();	
		Value value = JSObjectKJSValue((JSUserObject*)jsValue);
#if JAG_PINK_OR_LATER
		Object objValue = ptr->GetValue().toObject(exec);
		objValue.put(exec, CFStringToIdentifier(propertyName), value);
#else
		Object objValue = ptr->GetValue().imp()->toObject(exec);
		objValue.put(exec, CFStringToUString(propertyName), value);
#endif
	}
}

JSObjectRef JSValueWrapper::JSObjectCallFunction(void* data, JSObjectRef thisObj, CFArrayRef args)
{
    InterpreterLock lock;

	JSObjectRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = getThreadGlobalExecState();	
	
		Value value = JSObjectKJSValue((JSUserObject*)thisObj);
#if JAG_PINK_OR_LATER
		Object ksjThisObj = value.toObject(exec);
		Object objValue = ptr->GetValue().toObject(exec);
#else
		Object ksjThisObj = value.imp()->toObject(exec);
		Object objValue = ptr->GetValue().imp()->toObject(exec);
#endif

		List listArgs;
		CFIndex argCount = args ? CFArrayGetCount(args) : 0;
		for (CFIndex i = 0; i < argCount; i++)
		{
			JSObjectRef jsArg = (JSObjectRef)CFArrayGetValueAtIndex(args, i);
			Value kgsArg = JSObjectKJSValue((JSUserObject*)jsArg);
			listArgs.append(kgsArg);
		}

		Value resultValue = objValue.call(exec, ksjThisObj, listArgs);
		JSValueWrapper* wrapperValue = new JSValueWrapper(resultValue);
		JSObjectCallBacks callBacks;
		GetJSObectCallBacks(callBacks);
		result = JSObjectCreate(wrapperValue, &callBacks);
		if (!result)
		{
			delete wrapperValue;
		}
	}
	return result;
}

CFTypeRef JSValueWrapper::JSObjectCopyCFValue(void* data)
{
    InterpreterLock lock;

	CFTypeRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		result = KJSValueToCFType(ptr->fValue, getThreadGlobalExecState());
	}
	return result;
}

void JSValueWrapper::JSObjectMark(void* data)
{
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ptr->fValue.imp()->mark();
	}
}
