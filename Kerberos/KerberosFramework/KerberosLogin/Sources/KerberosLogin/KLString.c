/*
 * KLString.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLString.c,v 1.10 2003/08/22 16:21:02 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

// ---------------------------------------------------------------------------

KLStatus __KerberosLoginError (KLStatus inError, char *file, int line)
{
    KLStatus err = inError;
    
    switch (err) {
        case ccNoError:
            err = klNoErr;
            break;

        case ccErrBadName:
            err = klPrincipalDoesNotExistErr;
            break;

        case ccErrCCacheNotFound:
            err = klCacheDoesNotExistErr;
            break;

        case ccErrCredentialsNotFound:
            err = klNoCredentialsErr;
            break;

        case ccErrNoMem:
            err = klMemFullErr;
            break;
			
        case ccErrBadCredentialsVersion:
            err = klInvalidVersionErr;
            break;

        case ccErrBadParam:
        case ccIteratorEnd:
        case ccErrInvalidContext:
        case ccErrInvalidCCache:
        case ccErrInvalidString:
        case ccErrInvalidCredentials:
        case ccErrInvalidCCacheIterator:
        case ccErrInvalidCredentialsIterator:
        case ccErrInvalidLock:
        case ccErrBadAPIVersion:
        case ccErrContextLocked:
        case ccErrContextUnlocked:
        case ccErrCCacheLocked:
        case ccErrCCacheUnlocked:
        case ccErrBadLockType:
        case ccErrNeverDefault:
            err = klParameterErr;
            break;
	}
#if MACDEV_DEBUG
    if (err != klNoErr) {
        dprintf ("KLError_ (%ld) (remapped to %ld '%s') at %s: %d\n", 
                 inError, err, error_message (err), file, line);
    }
#endif
    return err;
}

// ---------------------------------------------------------------------------
KLStatus __KLRemapKerberos4Error (int inError)
{
    KLStatus err = inError;

    if ((err > 0) && (err < MAX_KRB_ERRORS)) {
        return err + ERROR_TABLE_BASE_krb;
    }
#if MACDEV_DEBUG
    if (err != klNoErr) {
        dprintf ("__KLRemapKerberos4Error (%ld) remapped to %ld '%s'\n",
                 inError, err, error_message (err));
    }
#endif
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLCreateString (const char *inString, char **outString)
{
    KLStatus err = klNoErr;
    
    if (inString  == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        *outString = (char *) calloc (strlen (inString) + 1, sizeof (char));
        if (*outString == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        strcpy (*outString, inString);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLCreateStringFromCFString (CFStringRef inString, char **outString)
{
    KLStatus err = klNoErr;
    char *string = NULL;
    CFIndex stringLength = 0;
    CFStringEncoding encoding = __KLApplicationGetTextEncoding ();
    
    if (inString  == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        stringLength = CFStringGetMaximumSizeForEncoding (CFStringGetLength (inString), encoding) + 1;
    }

    if (err == klNoErr) {
        string = (char *) calloc (stringLength, sizeof (char));
        if (string == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        if (CFStringGetCString (inString, string, stringLength, encoding) != true) {
            err = KLError_ (klMemFullErr);
        }
    }

    if (err == klNoErr) {
        *outString = string;
    } else {
        free (string);
    }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLCreateStringFromBuffer (const char *inBuffer, KLIndex inBufferLength, char **outString)
{
    KLStatus err = klNoErr;
    
    if (inBuffer  == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        *outString = (char *) calloc (inBufferLength + 1, sizeof (char));
        if (*outString == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        memcpy (*outString, inBuffer, inBufferLength * sizeof (char));
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLAddPrefixToString (const char *inPrefix, char **ioString)
{
    KLStatus err = klNoErr;
    char *string = NULL;

    if (inPrefix == NULL)                      { err = KLError_ (klParameterErr); }
    if (ioString == NULL || *ioString == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        string = (char *) calloc (strlen (inPrefix) + strlen (*ioString) + 1, sizeof (char));
        if (string == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr)  {
        KLIndex prefixLength = strlen (inPrefix);

        memcpy (string, inPrefix, prefixLength * sizeof (char));
        strcpy (string + prefixLength, *ioString);

        free (*ioString);
        *ioString = string;
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLAppendToString (const char *inAppendString, char **ioString)
{
    KLStatus err = klNoErr;
    char *string = NULL;

    if (inAppendString == NULL)                { err = KLError_ (klParameterErr); }
    if (ioString == NULL || *ioString == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        string = (char *) calloc (strlen (*ioString) + strlen (inAppendString) + 1, sizeof (char));
        if (string == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr)  {
        KLIndex previousLength = strlen (*ioString);

        memcpy (string, *ioString, previousLength * sizeof (char));
        strcpy (string + previousLength, inAppendString);
        
        free (*ioString);
        *ioString = string;
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLDisposeString (char *ioString)
{
    KLStatus err = klNoErr;
    
    if (ioString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        memset (ioString, 0, strlen (ioString));
        free (ioString);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------
typedef struct OpaqueKLStringArray {
    size_t count;
    size_t size;
    char **strings;
} StringArray;

StringArray kEmptyStringArray = { 0, 0, NULL };
const KLStringArray kKLEmptyStringArray = &kEmptyStringArray;

// Allocate in chunks so we aren't constantly reallocing:
#define kStringArraySizeIncrement 5

// ---------------------------------------------------------------------------
KLStatus __KLCreateStringArray (KLStringArray *outArray)
{
    KLStatus err = klNoErr;
    KLStringArray array = NULL;
    
    if (outArray == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        array = (KLStringArray) malloc (sizeof (StringArray));
        if (array == NULL) { err = KLError_ (klMemFullErr); }
    }

    if (err == klNoErr) {
        array->strings = NULL;
        array->count = 0;
        array->size = 0;
        *outArray = array;
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
KLStatus __KLCreateStringArrayFromStringArray (KLStringArray inArray, KLStringArray *outArray)
{
    KLStatus err = klNoErr;
    KLStringArray array = NULL;
    KLIndex count;
    KLIndex i;

    if (inArray  == NULL) { err = KLError_ (klParameterErr); }
    if (outArray == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLStringArrayGetStringCount (inArray, &count);
    }
    
    if (err == klNoErr) {
        err = __KLCreateStringArray (&array);
    }
        
    if (err == klNoErr) {
        for (i = 0; i < count; i++) {
            if (err == klNoErr) {
                err = __KLStringArrayAddString (array, inArray->strings[i]);
            }
        }
    }
    
    if (err == klNoErr) {
        *outArray = array;
        array = NULL;
    }

    if (array != NULL) { __KLDisposeStringArray (array); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArrayGetStringCount (KLStringArray inArray, KLIndex *outCount)
{
    if (inArray  == NULL) { return KLError_ (klParameterErr); }
    if (outCount == NULL) { return KLError_ (klParameterErr); }
    
    *outCount = inArray->count;
    
    return KLError_ (klNoErr);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArrayGetStringAtIndex (KLStringArray inArray, KLIndex inIndex, char **outString)
{
    KLStatus err = klNoErr;
    
    if (inArray   == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        if ((inIndex < 0) || (inIndex > inArray->count)) {
            err = KLError_ (klParameterErr);
        }
    }
    
    if (err == klNoErr) {
        *outString = inArray->strings[inIndex];
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArrayCopyStringAtIndex (KLStringArray inArray, KLIndex inIndex, char **outString)
{
    KLStatus err = klNoErr;
    
    if (inArray   == NULL) { err = KLError_ (klParameterErr); }
    if (outString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        if ((inIndex < 0) || (inIndex > inArray->count)) {
            err = KLError_ (klParameterErr);
        }
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (inArray->strings[inIndex], outString);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArrayGetIndexForString (KLStringArray inArray, const char *inString, KLIndex *outIndex)
{
    size_t i;
    
    if (inArray == NULL) { return KLError_ (klParameterErr); }

    for (i = 0; i < inArray->count; i++) {
        if (strcmp (inString, inArray->strings[i]) == 0) {
            *outIndex = i;
            return KLError_ (klNoErr);
        }
    }
    
    return KLError_ (klParameterErr);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArraySetStringAtIndex (KLStringArray inArray, const char *inString, KLIndex inIndex)
{
    KLStatus err = klNoErr;
    char *string = NULL;
    
    if (inArray  == NULL) { err = KLError_ (klParameterErr); }
    if (inString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        if (inIndex >= inArray->count) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (inString, &string);
    }
    
    if (err == klNoErr) {
        if (inArray->strings[inIndex] != NULL) { KLDisposeString (inArray->strings[inIndex]); }
        inArray->strings[inIndex] = string;
        string = NULL;
    }

    if (string != NULL) { KLDisposeString (string); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
KLStatus __KLStringArrayInsertStringBeforeIndex (KLStringArray inArray, const char *inString, KLIndex inIndex)
{
    KLStatus err = klNoErr;
    char *string = NULL;
    
    if (inArray  == NULL) { err = KLError_ (klParameterErr); }
    if (inString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        if ((inArray->count + 1) > inArray->size) {
            size_t newSize = inArray->size + kStringArraySizeIncrement;
            char **newStrings = (char **) realloc (inArray->strings, (sizeof (char *)) * newSize);
            if (newStrings == NULL) {
                err = KLError_ (klMemFullErr);
            } else {
                inArray->strings = newStrings;
                inArray->size = newSize;
            }
        }
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (inString, &string);
    }

    if (err == klNoErr) {
        if (inIndex < inArray->count) {
            memmove (&inArray->strings[inIndex + 1], &inArray->strings[inIndex], (inArray->count - inIndex) * sizeof (char *));
            inArray->strings[inIndex] = string; // add in the middle
        } else {
            inArray->strings[inArray->count] = string; // add to the end
        }
        string = NULL;
        inArray->count++;
    }

    if (string != NULL) { KLDisposeString (string); }
    
    return KLError_ (err);
}


// ---------------------------------------------------------------------------
KLStatus __KLStringArrayAddString (KLStringArray inArray, const char *inString)
{
    if (inArray == NULL) { return KLError_ (klParameterErr); }
    if (inString == NULL) { return KLError_ (klParameterErr); }

    return __KLStringArrayInsertStringBeforeIndex (inArray, inString, inArray->count);
}


// ---------------------------------------------------------------------------
KLStatus __KLStringArrayRemoveStringAtIndex (KLStringArray inArray, KLIndex inIndex)
{
    KLStatus err = klNoErr;

    if (inArray == NULL)  { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        if (inIndex >= inArray->count) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        char *string = inArray->strings[inIndex];
        
        memmove (&inArray->strings[inIndex], &inArray->strings[inIndex + 1], ((inArray->count - 1) - inIndex) * sizeof (char *));
        inArray->count--;
        free (string);
    }
        
    return KLError_ (err);    
}


// ---------------------------------------------------------------------------
KLStatus __KLDisposeStringArray (KLStringArray inArray)
{	
    KLStatus err = klNoErr;
    
    if (inArray == NULL) { err = KLError_ (klParameterErr); }
  
    if (err == klNoErr) {
        size_t i;
        
        for (i = 0; i < inArray->count; i++) {
            if (inArray->strings[i] != NULL) { free (inArray->strings[i]); }
        }
        free (inArray);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------
KLStatus __KLGetLocalizedString (const char *inKeyString, char **outString)
{
    KLStatus err = klNoErr;
    CFStringRef key = NULL;
    CFStringRef value = NULL;
    
    if (inKeyString == NULL || outString == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        key = CFStringCreateWithCString (kCFAllocatorDefault, inKeyString, kCFStringEncodingASCII);
        if (key == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    // Try to find the key, first searching in the framework, then in the main bundle
    if (err == klNoErr) {
        CFBundleRef frameworkBundle = CFBundleGetBundleWithIdentifier (CFSTR ("edu.mit.Kerberos"));
        if (frameworkBundle != NULL) {
            value = (CFStringRef) CFBundleGetValueForInfoDictionaryKey (frameworkBundle, key);
        }
        
        if (value == NULL) {
            CFBundleRef mainBundle = CFBundleGetMainBundle ();
            if (mainBundle != NULL) {
                value = (CFStringRef) CFBundleGetValueForInfoDictionaryKey (mainBundle, key);
            }
        }
    }
    
    // If we got a key, try to pull it out
    if (err == klNoErr) {
        if ((value != NULL) && (CFGetTypeID (value) == CFStringGetTypeID ())) {
            err = __KLCreateStringFromCFString (value, outString);
            if (err == klNoErr) {
                dprintf ("KLGetLocalizedString: Looking up key \"%s\" returned \"%s\"\n", inKeyString, *outString);
            }
        } else {
            err = __KLCreateString (inKeyString, outString);
        }
    }

    if (key != NULL) { CFRelease (key); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetApplicationNameString (char **outApplicationName)
{
    KLStatus err = klNoErr;
    Str255 name;

    if (outApplicationName == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        ProcessSerialNumber currentProcessSN = {0L, kCurrentProcess};
        ProcessInfoRec info;

        info.processInfoLength = sizeof (ProcessInfoRec);
        info.processName = name;
        info.processAppSpec = NULL;

        err = GetProcessInformation (&currentProcessSN, &info);
    }

    if (err == klNoErr) {
        err = __KLCreateStringFromBuffer (&name[1], name[0], outApplicationName);
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

// GAAAAAH!
// This algorithm was pirated from the Security framework.
// If it stops working, check what they are doing.  See OSXCode::main()

// Note: The public CFURLCopyFileSystemPath fails to resolve relative URLs as
// produced by CFURL methods. We need to call an internal(!) method of CF to get
// the full path.
extern CFStringRef CFURLCreateStringWithFileSystemPath(CFAllocatorRef allocator,
                                                       CFURLRef anURL, CFURLPathStyle fsType, 
                                                       Boolean resolveAgainstBase);

KLStatus __KLGetApplicationIconPathString (char **outApplicationIconPath)
{
    KLStatus err = klNoErr;
    char *applicationIconPath = NULL;
    CFStringRef iconPathString = NULL;
    CFURLRef bundleURL = NULL;
    CFURLRef resourcesURL = NULL;
    CFURLRef executableURL = NULL;
    
    if (outApplicationIconPath == NULL) { err = KLError_ (klParameterErr); }

    // First, try to see if it is a real
    if (err == klNoErr) {
        CFBundleRef mainBundle = CFBundleGetMainBundle ();
        if (mainBundle != NULL) {
            bundleURL = CFBundleCopyBundleURL (mainBundle);
            resourcesURL = CFBundleCopyResourcesDirectoryURL (mainBundle);
            executableURL = CFBundleCopyExecutableURL (mainBundle);

            // Is it a bundle?
            if ((bundleURL != NULL) && (resourcesURL != NULL)) {
                if (!CFEqual (bundleURL, resourcesURL)) {
                    iconPathString = CFURLCreateStringWithFileSystemPath (kCFAllocatorDefault, bundleURL,
                                                                          kCFURLPOSIXPathStyle, true);
                } else {
                    iconPathString = CFURLCreateStringWithFileSystemPath (kCFAllocatorDefault, executableURL,
                                                                          kCFURLPOSIXPathStyle, true);
                }
                
                if (iconPathString == NULL) {
                    err = KLError_ (klMemFullErr); // couldn't write
                } else {
                    err = __KLCreateStringFromCFString (iconPathString, &applicationIconPath);
                }
            }
        }
    }
    
    // If we haven't gotten a string and we haven't gotten a fatal error, try dyld
    // Note that this doesn't work on CFM apps because you get a path to LaunchCFMApp
    if ((err == klNoErr) && (applicationIconPath == NULL)) {
        char *pathBuffer = NULL;
        unsigned long pathSize = 0;
    
        // Make a tiny stupid buffer to get the length of the path
        if (err == klNoErr) {
            pathBuffer = malloc (1);
            if (pathBuffer == NULL) { err = KLError_ (klMemFullErr); }
        }
        
        // Get the length of the path
        if (err == klNoErr) {
            if (_NSGetExecutablePath (pathBuffer, &pathSize) != 0) {
                char *tempBuffer = realloc (pathBuffer, pathSize + 1);
                if (tempBuffer == NULL) {
                    err = KLError_ (klMemFullErr);
                } else {
                    pathBuffer = tempBuffer;
                }
            }
        }
        
        // Get the path
        if (err == klNoErr) {
            if (_NSGetExecutablePath (pathBuffer, &pathSize) != 0) {
                err = KLError_ (klMemFullErr);
            } else {
                err = __KLCreateString (pathBuffer, &applicationIconPath);
            }
        }
        
        if (pathBuffer != NULL) { free (pathBuffer); }
    }
    
    if (err == klNoErr) {
        dprintf ("__KLGetApplicationIconPathString returning path '%s'\n", applicationIconPath);
        *outApplicationIconPath = applicationIconPath;
        applicationIconPath = NULL;
    }
    
    if (bundleURL           != NULL) { CFRelease (bundleURL); }
    if (resourcesURL        != NULL) { CFRelease (resourcesURL); }
    if (executableURL       != NULL) { CFRelease (executableURL); }
    if (iconPathString      != NULL) { CFRelease (iconPathString); }
    if (applicationIconPath != NULL) { KLDisposeString (applicationIconPath); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

CFStringEncoding __KLApplicationGetTextEncoding (void)
{
    typedef TextEncoding (*GetApplicationTextEncodingProcPtr) (void);
    GetApplicationTextEncodingProcPtr GetApplicationTextEncodingPtr = NULL;

    if (__KLIsKerberosLoginServer ()) {
        return kCFStringEncodingUTF8;  // KerberosLoginServer only does UTF8
    }

    CFBundleRef carbonBundle = CFBundleGetBundleWithIdentifier (CFSTR ("com.apple.Carbon"));
    if (carbonBundle != NULL && CFBundleIsExecutableLoaded (carbonBundle)) {
        GetApplicationTextEncodingPtr = (GetApplicationTextEncodingProcPtr) CFBundleGetFunctionPointerForName (carbonBundle,
                                                                                                               CFSTR ("GetApplicationTextEncoding"));
    }

    if (GetApplicationTextEncodingPtr != NULL) {
        return (CFStringEncoding) (*GetApplicationTextEncodingPtr) ();
    }

    return CFStringGetSystemEncoding ();
}

#pragma mark -


// ---------------------------------------------------------------------------

KLStatus KLGetErrorString (KLStatus inError, char **outErrorString)
{
    KLStatus err = klNoErr;
    char *errorString = NULL;
    const char *comErrString = NULL;
    
    switch (inError) {
        case KRBET_INTK_BADPW:			// krb4 password incorrect
        case KRB5KRB_AP_ERR_BAD_INTEGRITY:	// krb5 has a stupid error message for this
            err = __KLGetLocalizedString ("KLStringPasswordIncorrect", &errorString);
            break;
		
        case klCapsLockErr:
            err = __KLGetLocalizedString ("KLStringPasswordIncorrectCheckCapsLock", &errorString);
            break;
			
        case KRB5KDC_ERR_PREAUTH_FAILED:
            err = __KLGetLocalizedString ("KLStringPreauthenticationFailed", &errorString);
            break;
		
        case KRB5KRB_AP_ERR_SKEW:	// Krb5
        case KRBET_RD_AP_TIME:          // Krb4
            err = __KLGetLocalizedString ("KLStringClockSkewTooBig", &errorString);
            break;

        case KRBET_KDC_SERVICE_EXP: 	// kaserver (could mean that krbtgt expired, but that very very bad)
            err = __KLGetLocalizedString ("KLStringKaserverClockSkewTooBig", &errorString);
            break;
			
        default:
            // Use com_err for all errors (it also calls ErrorLib):
            comErrString = error_message (inError);
            if (comErrString != NULL) {
                err = __KLCreateString (comErrString, &errorString);
            }
            break;
    }
    
    if (err == klNoErr) {
        if (errorString == NULL) {
            err = __KLGetLocalizedString ("KLStringUnknownError", &errorString);
        }
    }
    
    if (err == klNoErr) {
        *outErrorString = errorString;
        errorString = NULL;
    }

    if (errorString != NULL) { KLDisposeString (errorString); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

#define kFiveMinutes (5*60)

KLStatus __KLVerifyKDCOffsetsForKerberos4 (const krb5_context inContext)
{
    KLStatus err = klNoErr;
    krb5_int32 seconds = 0;

    err = krb5_get_time_offsets (inContext, &seconds, NULL);
    if ((err == 0) && ((seconds > kFiveMinutes) || (seconds < -kFiveMinutes))) {
        err = KRBET_RD_AP_TIME;
    }
    
    return KLError_ (err);
}

