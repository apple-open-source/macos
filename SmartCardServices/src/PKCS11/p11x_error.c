/******************************************************************************
** 
**  $Id: p11x_error.c,v 1.2 2003/02/13 20:06:40 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Error handling
** 
******************************************************************************/

#include "cryptoki.h"

/******************************************************************************
** Function: error_LogCmd
**
** If there is an error then this logs it to the logfile as a stringified 
** text message.
**
** Parameters:
**  err         - CKR Error code
**  cond        - Test condition that says whether or not this is an error
**  file        - Filename where the error occured (__FILE__)
**  line        - Line number of the error (__LINE__)
**  stringifyFn - Function used to stringify the error code (err)
**
** Returns:
**  The error code that was passed in
*******************************************************************************/
CK_RV error_LogCmd(CK_RV err, CK_RV cond, CK_CHAR *file, CK_LONG line, char *(*stringifyFn)(CK_RV))
{
    if (err != cond)
        log_Log(LOG_MED, "(%s %ld): error: 0x%lX \"%s\"", file, line, err, stringifyFn(err));

    return err;
}

/******************************************************************************
** Function: error_Stringify
**
** Convert a CKR_XXX error code to text.
**
** Parameters:
**  rv - Error code to convert
**
** Returns:
**  A static string error message (make this const?)
*******************************************************************************/
char *error_Stringify(CK_RV rv)
{
    char *error;

    switch(rv)
    {
    case CKR_OK:
        error = "CKR_OK";
        break;

    case CKR_CANCEL:
        error = "CKR_CANCEL";
        break;

    case CKR_HOST_MEMORY:
        error = "CKR_HOST_MEMORY";
        break;

    case CKR_SLOT_ID_INVALID:
        error = "CKR_SLOT_ID_INVALID";
        break;

    case CKR_GENERAL_ERROR:
        error = "CKR_GENERAL_ERROR";
        break;

    case CKR_FUNCTION_FAILED:
        error = "CKR_FUNCTION_FAILED";
        break;

    case CKR_ARGUMENTS_BAD:
        error = "CKR_ARGUMENTS_BAD";
        break;

    case CKR_NO_EVENT:
        error = "CKR_NO_EVENT";
        break;

    case CKR_NEED_TO_CREATE_THREADS:
        error = "CKR_NEED_TO_CREATE_THREADS";
        break;

    case CKR_CANT_LOCK:
        error = "CKR_CANT_LOCK";
        break;

    case CKR_ATTRIBUTE_READ_ONLY:
        error = "CKR_ATTRIBUTE_READ_ONLY";
        break;

    case CKR_ATTRIBUTE_SENSITIVE:
        error = "CKR_ATTRIBUTE_SENSITIVE";
        break;

    case CKR_ATTRIBUTE_TYPE_INVALID:
        error = "CKR_ATTRIBUTE_TYPE_INVALID";
        break;

    case CKR_ATTRIBUTE_VALUE_INVALID:
        error = "CKR_ATTRIBUTE_VALUE_INVALID";
        break;

    case CKR_DATA_INVALID:
        error = "CKR_DATA_INVALID";
        break;

    case CKR_DATA_LEN_RANGE:
        error = "CKR_DATA_LEN_RANGE";
        break;

    case CKR_DEVICE_ERROR:
        error = "CKR_DEVICE_ERROR";
        break;

    case CKR_DEVICE_MEMORY:
        error = "CKR_DEVICE_MEMORY";
        break;

    case CKR_DEVICE_REMOVED:
        error = "CKR_DEVICE_REMOVED";
        break;

    case CKR_ENCRYPTED_DATA_INVALID:
        error = "CKR_ENCRYPTED_DATA_INVALID";
        break;

    case CKR_ENCRYPTED_DATA_LEN_RANGE:
        error = "CKR_ENCRYPTED_DATA_LEN_RANGE";
        break;

    case CKR_FUNCTION_CANCELED:
        error = "CKR_FUNCTION_CANCELED";
        break;

    case CKR_FUNCTION_NOT_PARALLEL:
        error = "CKR_FUNCTION_NOT_PARALLEL";
        break;

    case CKR_FUNCTION_NOT_SUPPORTED:
        error = "CKR_FUNCTION_NOT_SUPPORTED";
        break;

    case CKR_KEY_HANDLE_INVALID:
        error = "CKR_KEY_HANDLE_INVALID";
        break;

    case CKR_KEY_SIZE_RANGE:
        error = "CKR_KEY_SIZE_RANGE";
        break;

    case CKR_KEY_TYPE_INCONSISTENT:
        error = "CKR_KEY_TYPE_INCONSISTENT";
        break;

    case CKR_KEY_NOT_NEEDED:
        error = "CKR_KEY_NOT_NEEDED";
        break;

    case CKR_KEY_CHANGED:
        error = "CKR_KEY_CHANGED";
        break;

    case CKR_KEY_NEEDED:
        error = "CKR_KEY_NEEDED";
        break;

    case CKR_KEY_INDIGESTIBLE:
        error = "CKR_KEY_INDIGESTIBLE";
        break;

    case CKR_KEY_FUNCTION_NOT_PERMITTED:
        error = "CKR_KEY_FUNCTION_NOT_PERMITTED";
        break;

    case CKR_KEY_NOT_WRAPPABLE:
        error = "CKR_KEY_NOT_WRAPPABLE";
        break;

    case CKR_KEY_UNEXTRACTABLE:
        error = "CKR_KEY_UNEXTRACTABLE";
        break;

    case CKR_MECHANISM_INVALID:
        error = "CKR_MECHANISM_INVALID";
        break;

    case CKR_MECHANISM_PARAM_INVALID:
        error = "CKR_MECHANISM_PARAM_INVALID";
        break;

    case CKR_OBJECT_HANDLE_INVALID:
        error = "CKR_OBJECT_HANDLE_INVALID";
        break;

    case CKR_OPERATION_ACTIVE:
        error = "CKR_OPERATION_ACTIVE";
        break;

    case CKR_OPERATION_NOT_INITIALIZED:
        error = "CKR_OPERATION_NOT_INITIALIZED";
        break;

    case CKR_PIN_INCORRECT:
        error = "CKR_PIN_INCORRECT";
        break;

    case CKR_PIN_INVALID:
        error = "CKR_PIN_INVALID";
        break;

    case CKR_PIN_LEN_RANGE:
        error = "CKR_PIN_LEN_RANGE";
        break;

    case CKR_PIN_EXPIRED:
        error = "CKR_PIN_EXPIRED";
        break;

    case CKR_PIN_LOCKED:
        error = "CKR_PIN_LOCKED";
        break;

    case CKR_SESSION_CLOSED:
        error = "CKR_SESSION_CLOSED";
        break;

    case CKR_SESSION_COUNT:
        error = "CKR_SESSION_COUNT";
        break;

    case CKR_SESSION_HANDLE_INVALID:
        error = "CKR_SESSION_HANDLE_INVALID";
        break;

    case CKR_SESSION_PARALLEL_NOT_SUPPORTED:
        error = "CKR_SESSION_PARALLEL_NOT_SUPPORTED";
        break;

    case CKR_SESSION_READ_ONLY:
        error = "CKR_SESSION_READ_ONLY";
        break;

    case CKR_SESSION_EXISTS:
        error = "CKR_SESSION_EXISTS";
        break;

    case CKR_SESSION_READ_ONLY_EXISTS:
        error = "CKR_SESSION_READ_ONLY_EXISTS";
        break;

    case CKR_SESSION_READ_WRITE_SO_EXISTS:
        error = "CKR_SESSION_READ_WRITE_SO_EXISTS";
        break;

    case CKR_SIGNATURE_INVALID:
        error = "CKR_SIGNATURE_INVALID";
        break;

    case CKR_SIGNATURE_LEN_RANGE:
        error = "CKR_SIGNATURE_LEN_RANGE";
        break;

    case CKR_TEMPLATE_INCOMPLETE:
        error = "CKR_TEMPLATE_INCOMPLETE";
        break;

    case CKR_TEMPLATE_INCONSISTENT:
        error = "CKR_TEMPLATE_INCONSISTENT";
        break;

    case CKR_TOKEN_NOT_PRESENT:
        error = "CKR_TOKEN_NOT_PRESENT";
        break;

    case CKR_TOKEN_NOT_RECOGNIZED:
        error = "CKR_TOKEN_NOT_RECOGNIZED";
        break;

    case CKR_TOKEN_WRITE_PROTECTED:
        error = "CKR_TOKEN_WRITE_PROTECTED";
        break;

    case CKR_UNWRAPPING_KEY_HANDLE_INVALID:
        error = "CKR_UNWRAPPING_KEY_HANDLE_INVALID";
        break;

    case CKR_UNWRAPPING_KEY_SIZE_RANGE:
        error = "CKR_UNWRAPPING_KEY_SIZE_RANGE";
        break;

    case CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT:
        error = "CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT";
        break;

    case CKR_USER_ALREADY_LOGGED_IN:
        error = "CKR_USER_ALREADY_LOGGED_IN";
        break;

    case CKR_USER_NOT_LOGGED_IN:
        error = "CKR_USER_NOT_LOGGED_IN";
        break;

    case CKR_USER_PIN_NOT_INITIALIZED:
        error = "CKR_USER_PIN_NOT_INITIALIZED";
        break;

    case CKR_USER_TYPE_INVALID:
        error = "CKR_USER_TYPE_INVALID";
        break;

    case CKR_USER_ANOTHER_ALREADY_LOGGED_IN:
        error = "CKR_USER_ANOTHER_ALREADY_LOGGED_IN";
        break;

    case CKR_USER_TOO_MANY_TYPES:
        error = "CKR_USER_TOO_MANY_TYPES";
        break;

    case CKR_WRAPPED_KEY_INVALID:
        error = "CKR_WRAPPED_KEY_INVALID";
        break;

    case CKR_WRAPPED_KEY_LEN_RANGE:
        error = "CKR_WRAPPED_KEY_LEN_RANGE";
        break;

    case CKR_WRAPPING_KEY_HANDLE_INVALID:
        error = "CKR_WRAPPING_KEY_HANDLE_INVALID";
        break;

    case CKR_WRAPPING_KEY_SIZE_RANGE:
        error = "CKR_WRAPPING_KEY_SIZE_RANGE";
        break;

    case CKR_WRAPPING_KEY_TYPE_INCONSISTENT:
        error = "CKR_WRAPPING_KEY_TYPE_INCONSISTENT";
        break;

    case CKR_RANDOM_SEED_NOT_SUPPORTED:
        error = "CKR_RANDOM_SEED_NOT_SUPPORTED";
        break;

    case CKR_RANDOM_NO_RNG:
        error = "CKR_RANDOM_NO_RNG";
        break;

    case CKR_DOMAIN_PARAMS_INVALID:
        error = "CKR_DOMAIN_PARAMS_INVALID";
        break;

    case CKR_BUFFER_TOO_SMALL:
        error = "CKR_BUFFER_TOO_SMALL";
        break;

    case CKR_SAVED_STATE_INVALID:
        error = "CKR_SAVED_STATE_INVALID";
        break;

    case CKR_INFORMATION_SENSITIVE:
        error = "CKR_INFORMATION_SENSITIVE";
        break;

    case CKR_STATE_UNSAVEABLE:
        error = "CKR_STATE_UNSAVEABLE";
        break;

    case CKR_CRYPTOKI_NOT_INITIALIZED:
        error = "CKR_CRYPTOKI_NOT_INITIALIZED";
        break;

    case CKR_CRYPTOKI_ALREADY_INITIALIZED:
        error = "CKR_CRYPTOKI_ALREADY_INITIALIZED";
        break;

    case CKR_MUTEX_BAD:
        error = "CKR_MUTEX_BAD";
        break;

    case CKR_MUTEX_NOT_LOCKED:
        error = "CKR_MUTEX_NOT_LOCKED";
        break;

    case CKR_VENDOR_DEFINED:
        error = "CKR_VENDOR_DEFINED";
        break;

    default:
        error = "Unknown CKR error";
        break;
    }

    return error;
}

