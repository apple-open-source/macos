/*
 * Copyright (c) 2001 Apple Computer, Inc. All Rights Reserved.
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


/*
 *  AuthorizationPlugin.h
 *  AuthorizationPlugin -- APIs for implementing authorization plugins.
 */

#if !defined(__AuthorizationPlugin__)
#define __AuthorizationPlugin__ 1

#include <Security/Authorization.h>

#if defined(__cplusplus)
extern "C" {
#endif


/*!
	@header AuthorizationPlugin
	Version 0.3 05/09/2001

	Foo bar @@@.

*/


/*!
	@typedef AuthorizationValue
	@@@
*/
typedef struct AuthorizationValue
{
    UInt32 length;
    void *data;
} AuthorizationValue;

typedef struct AuthorizationValueVector
{
	UInt32 count;
	AuthorizationValue *values;
} AuthorizationValueVector;

typedef UInt32 AuthorizationContextFlags;
enum
{
    /* If set, it will be possible to obtain the value of this attribute usingAuthorizationCopyInfo(). */
    kAuthorizationContextFlagExtractable = (1 << 0),

    /* If set, this value will not be remembered in a "credential".  @@@ Do we need this? */
    kAuthorizationContextFlagVolatile = (1 << 1)
};


/*!
	@typedef AuthorizationMechanismId
	@@@
*/
typedef const AuthorizationString AuthorizationMechanismId;

/*!
	@typedef AuthorizationPluginRef
	An instance of a plugin (even though there will probably only be one).
*/
typedef void *AuthorizationPluginRef;

/*!
	@typedef AuthorizationMechanismRef
	An instance of a mechanism in a plugin.
*/
typedef void *AuthorizationMechanismRef;

/*!
	@typedef AuthorizationEngineRef
	The engines handle for an instance of a mechanism in a plugin (corresponds to a particular AuthorizationMechanismRef).
*/
typedef struct __OpaqueAuthorizationEngine *AuthorizationEngineRef;


/*!
	@typedef AuthorizationSessionId
	A unique value for an AuthorizationSession being evaluated, provided by the authorization engine.
    A session is represented by a top level call to an Authorization API.
    @@@ Should this be changed to tie a session to the lifetime of an AuthorizationRef?  -- Michael
*/
typedef void *AuthorizationSessionId;

/*!
	@typedef AuthorizationResult
	Possible values that SetResult may use.

    @param kAuthorizationResultAllow the operation succeeded and should be allowed as far as this mechanism is concerned.
    @param kAuthorizationResultDeny the operation succeeded and should be denied as far as this mechanism is concerned.
    @param kAuthorizationResultUndefined the operation failed for some reason and should not be retried for this session.
    @param kAuthorizationResultUserCanceled the user has requested that the evaluation be terminated.
*/
typedef UInt32 AuthorizationResult;
enum
{
    kAuthorizationResultAllow,
    kAuthorizationResultDeny,
    kAuthorizationResultUndefined,
    kAuthorizationResultUserCanceled,
};

enum {
    kAuthorizationPluginInterfaceVersion = 0,
};

enum {
    kAuthorizationCallbacksVersion = 0,
};


/* Callback API of the AuthorizationEngine. */
typedef struct AuthorizationCallbacks {
    /* Will be set to kAuthorizationCallbacksVersion. */
    UInt32 version;

    /* Flow control */

    /* Set a result after a call to AuthorizationSessionInvoke. */
    OSStatus (*SetResult)(AuthorizationEngineRef inEngine, AuthorizationResult inResult);

    /* Request authorization engine to interrupt all mechamisms invoked after this mechamism has called SessionSetResult and then call AuthorizationSessionInvoke again. */
    OSStatus (*RequestInterrupt)(AuthorizationEngineRef inEngine);

    OSStatus (*DidDeactivate)(AuthorizationEngineRef inEngine);


    /* Getters and setters */
    OSStatus (*GetContextValue)(AuthorizationEngineRef inEngine,
        AuthorizationString inKey,
        AuthorizationContextFlags *outContextFlags,
        const AuthorizationValue **outValue);

    OSStatus (*SetContextValue)(AuthorizationEngineRef inEngine,
        AuthorizationString inKey,
        AuthorizationContextFlags inContextFlags,
        const AuthorizationValue *inValue);

    OSStatus (*GetHintValue)(AuthorizationEngineRef inEngine,
        AuthorizationString inKey,
        const AuthorizationValue **outValue);

    OSStatus (*SetHintValue)(AuthorizationEngineRef inEngine,
        AuthorizationString inKey,
        const AuthorizationValue *inValue);

    OSStatus (*GetArguments)(AuthorizationEngineRef inEngine,
        const AuthorizationValueVector **outArguments);

    OSStatus (*GetSessionId)(AuthorizationEngineRef inEngine,
        AuthorizationSessionId *outSessionId);


} AuthorizationCallbacks;


/* Functions that must be implemented by each plugin. */

typedef struct AuthorizationPluginInterface
{
    /* Must be set to kAuthorizationPluginInterfaceVersion. */
    UInt32 version;

    /* Notify a plugin that it is about to be unloaded so it get a chance to clean up and release any resources it is holding.  */
    OSStatus (*PluginDestroy)(AuthorizationPluginRef inPlugin);

    /* The plugin should create an AuthorizationMechanismRef and remeber inEngine, mechanismId and callbacks for future reference.  It is guaranteed that MechanismDestroy will be called on the returned AuthorizationMechanismRef sometime after this function.  */
    OSStatus (*MechanismCreate)(AuthorizationPluginRef inPlugin,
        AuthorizationEngineRef inEngine,
        AuthorizationMechanismId mechanismId,
        AuthorizationMechanismRef *outMechanism);

    /* Invoke (or evaluate) an instance of a mechanism (created with MechanismCreate).  It should call SetResult during or after returning from this function.  */
    OSStatus (*MechanismInvoke)(AuthorizationMechanismRef inMechanism);

    /* Plugin should respond with a SessionDidDeactivate asap. */
    OSStatus (*MechanismDeactivate)(AuthorizationMechanismRef inMechanism);

    OSStatus (*MechanismDestroy)(AuthorizationMechanismRef inMechanism);

} AuthorizationPluginInterface;


/* @function AuthorizationPluginCreate

    Initialize a plugin after it gets loaded.  This is the main entry point to a plugin.  This function will only be called once and after all Mechanism instances have been destroyed outPluginInterface->PluginDestroy will be called.

    @param callbacks (input) A pointer to an AuthorizationCallbacks which contains the callbacks implemented by the AuthorizationEngine.
    @param outPlugin (output) On successful completion should contain a valid AuthorizationPluginRef.  This will be passed in to any subsequent calls the engine makes to  outPluginInterface->MechanismCreate and outPluginInterface->PluginDestroy.
    @param outPluginInterface (output) On successful completion should contain a pointer to a AuthorizationPluginInterface that will stay valid until outPluginInterface->PluginDestroy is called. */
OSStatus AuthorizationPluginCreate(const AuthorizationCallbacks *callbacks,
    AuthorizationPluginRef *outPlugin,
    const AuthorizationPluginInterface **outPluginInterface);

#if defined(__cplusplus)
}
#endif

#endif /* ! __AuthorizationPlugin__ */
