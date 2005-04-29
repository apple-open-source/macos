/*
 * KLPreferences.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLPreferences.c,v 1.19 2004/12/10 21:18:07 lxs Exp $
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

// Core Foundation Keys  (use #define because "CFSTR()" is a macro to a function)
#define kLoginLibraryPreferences       CFSTR("edu.mit.Kerberos.KerberosLogin")

#define kKLName                        CFSTR("KLName")
#define kKLInstance                    CFSTR("KLInstance")

#define kKLMinimumTicketLifetime       CFSTR("KLMinimumTicketLifetime")
#define kKLMaximumTicketLifetime       CFSTR("KLMaximumTicketLifetime")
#define kKLDefaultTicketLifetime       CFSTR("KLDefaultTicketLifetime")

#define kKLDefaultRenewableTicket      CFSTR("KLGetRenewableTickets")
#define kKLMinimumRenewableLifetime    CFSTR("KLMinimumRenewableLifetime")
#define kKLMaximumRenewableLifetime    CFSTR("KLMaximumRenewableLifetime")
#define kKLDefaultRenewableLifetime    CFSTR("KLDefaultRenewableLifetime")

#define kKLDefaultForwardableTicket    CFSTR("KLDefaultForwardableTicket")
#define kKLDefaultProxiableTicket      CFSTR("KLGetProxiableTickets")
#define kKLDefaultAddresslessTicket    CFSTR("KLGetAddresslessTickets")

#define kKLLongLifetimeDisplay         CFSTR("KLLongLifetimeDisplay")
#define kKLShowOptions                 CFSTR("KLShowOptions")
#define kKLRememberShowOptions         CFSTR("KLRememberShowOptions")
#define kKLRememberPrincipal           CFSTR("KLRememberPrincipal")
#define kKLRememberExtras              CFSTR("KLRememberExtras")

#define kKLFavoriteRealms              CFSTR("KLFavoriteRealms")
#define kKLDefaultRealm                CFSTR("KLDefaultRealm")


// Default values if the preference file is not available
const char *kDefaultLoginName                           = "";
const char *kDefaultLoginInstance                       = "";

const KLLifetime  kDefaultMinimumTicketLifetime         = 10*60;         // 10 minutes
const KLLifetime  kDefaultMaximumTicketLifetime         = 10*60*60;      // 10 hours
const KLLifetime  kDefaultTicketLifetime                = 10*60*60;      // 10 hours

const KLBoolean   kDefaultRenewableTicket               = true;
const KLLifetime  kDefaultMinimumRenewableLifetime      = 10*60;         // 10 minutes
const KLLifetime  kDefaultMaximumRenewableLifetime      = 7*24*60*60;    // 7 days
const KLLifetime  kDefaultRenewableLifetime             = 7*24*60*60;    // 7 days

const KLBoolean   kDefaultForwardableTicket             = true;
const KLBoolean   kDefaultProxiableTicket               = true;
const KLBoolean   kDefaultAddresslessTicket             = true;

const KLBoolean   kDefaultLongLifetimeDisplay           = true;
const KLBoolean   kDefaultShowOptions                   = false;
const KLBoolean   kDefaultRememberShowOptions           = true;
const KLBoolean   kDefaultRememberPrincipal             = true;
const KLBoolean   kDefaultRememberExtras                = true;

const KLIndex     kDefaultRealmCount                    = 0;
#define           kDefaultRealmList                     kKLEmptyStringArray
const char       *kNoDefaultRealm                       = "";           // empty default realm

/* Internal prototypes */

static KLStatus __KLPreferencesCopyValue (CFStringRef inKey, CFTypeID inValueType, CFPropertyListRef *outValue);
static KLStatus __KLPreferencesSetValue (CFStringRef inKey, CFPropertyListRef inValue);

static KLStatus __KLPreferencesGetStringWithKey (const CFStringRef inKey, const char *inDefaultString, char **outString);
static KLStatus __KLPreferencesSetStringWithKey (const CFStringRef inKey, const char *inString);

static KLStatus __KLPreferencesGetNumberWithKey (const CFStringRef inKey, u_int32_t inDefaultNumber, u_int32_t *outNumber);
static KLStatus __KLPreferencesSetNumberWithKey (const CFStringRef inKey, u_int32_t inNumber);

static KLStatus __KLPreferencesGetBooleanWithKey (const CFStringRef inKey, KLBoolean inDefaultBoolean, KLBoolean *outBoolean);
static KLStatus __KLPreferencesSetBooleanWithKey (const CFStringRef inKey, KLBoolean inBoolean);

static KLBoolean __KLPreferencesGetStringArrayWithKey (const CFStringRef inKey, const KLStringArray inDefaultStringArray, KLStringArray *outStringArray);
static KLStatus __KLPreferencesSetStringArrayWithKey (const CFStringRef inKey, const KLStringArray inStringArray);

static KLTime __KLPreferencesGetLibDefaultTime (const char *inLibdefaultName, KLTime inDefaultTime);

static KLStatus __KLPreferencesGetFavoriteRealmList (KLStringArray *outRealmList);
static KLStatus __KLPreferencesSetFavoriteRealmList (KLStringArray inRealmList);
static KLStatus __KLPreferencesGetKerberosDefaultRealm (char **outDefaultRealm);
static KLStatus __KLPreferencesEnsureKerberosDefaultRealmIsInFavorites (KLStringArray inRealmList);

/* Functions */

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesCopyValue (CFStringRef inKey, CFTypeID inValueType, CFPropertyListRef *outValue)
{
    KLStatus err = klNoErr;
    CFPropertyListRef value = NULL;
    
    if (inKey    == NULL) { err = KLError_ (klParameterErr); }
    if (outValue == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        if (__KLAllowHomeDirectoryAccess()) {
            if (value == NULL) {
                value = CFPreferencesCopyValue (inKey, kLoginLibraryPreferences,
                                                kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
            }
            if (value == NULL) {
                value = CFPreferencesCopyValue (inKey, kLoginLibraryPreferences,
                                                kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
            }
        }
        if (value == NULL) {
            value = CFPreferencesCopyValue (inKey, kLoginLibraryPreferences,
                                            kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        }
        if (value == NULL) {
            value = CFPreferencesCopyValue (inKey, kLoginLibraryPreferences,
                                            kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        }
        
        if ((value != NULL) && (CFGetTypeID (value) != inValueType)) {
            err = KLError_ (klPreferencesReadErr);  // prefs contain bogus value for this key
        }

    }
    
    if (err == klNoErr) {
        *outValue = (void *) value;
        value = NULL;
    }
    
    if (value != NULL) { CFRelease (value); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesSetValue (CFStringRef inKey, CFPropertyListRef inValue)
{
    KLStatus err = klNoErr;
    
    if (inKey   == NULL) { err = KLError_ (klParameterErr); }
    if (inValue == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        // Currently just do nothing if we are not allowed to touch to the user's homedir
        if (__KLAllowHomeDirectoryAccess()) {
            CFPreferencesSetValue (inKey, inValue, kLoginLibraryPreferences, 
                                   kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
            if (CFPreferencesSynchronize (kLoginLibraryPreferences,
                                          kCFPreferencesCurrentUser, kCFPreferencesAnyHost) == false) {
                err = KLError_ (klPreferencesWriteErr);
            }
        } else {
            CFPreferencesSetValue (inKey, inValue, kLoginLibraryPreferences, 
                                   kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
            if (CFPreferencesSynchronize (kLoginLibraryPreferences,
                                          kCFPreferencesAnyUser, kCFPreferencesCurrentHost) == false) {
                err = KLError_ (klPreferencesWriteErr);
            }
        }
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesGetStringWithKey (const CFStringRef inKey, const char *inDefaultString, char **outString)
{
    KLStatus err = klNoErr;
    CFStringRef value = NULL;

    if (inKey           == NULL) { err = KLError_ (klParameterErr); }
    if (inDefaultString == NULL) { err = KLError_ (klParameterErr); }
    if (outString       == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesCopyValue (inKey, CFStringGetTypeID (), (CFPropertyListRef *) &value);
    }
    
    if (err == klNoErr) {
        if ((value == NULL) || (CFGetTypeID (value) != CFStringGetTypeID ())) {
            err = __KLCreateString (inDefaultString, outString);
        } else {
            err = __KLCreateStringFromCFString (value, __KLApplicationGetTextEncoding(), outString);
        }
    }
    
    if (value != NULL) { CFRelease (value); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesSetStringWithKey (const CFStringRef inKey, const char *inString)
{
    KLStatus err = klNoErr;
    CFStringRef value = NULL;
    
    if (inKey    == NULL) { err = KLError_ (klParameterErr); }
    if (inString == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        value = CFStringCreateWithCString (kCFAllocatorDefault, inString, __KLApplicationGetTextEncoding ());
        if (value == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesSetValue (inKey, value);
    }
    
    if (value != NULL) { CFRelease (value); }
    
    return KLError_ (err);
}

#pragma mark -
// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesGetNumberWithKey (const CFStringRef inKey, u_int32_t inDefaultNumber, u_int32_t *outNumber)
{
    KLStatus err = klNoErr;
    CFNumberRef value = NULL;

    if (inKey == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesCopyValue (inKey, CFNumberGetTypeID (), (CFPropertyListRef *) &value);
    }
    
    if (err == klNoErr) {
        if (value == NULL) {
            *outNumber = inDefaultNumber;
        } else {
            SInt32 castWrapper; // CFNumbers are signed so we need to cast
            if (CFNumberGetValue (value, kCFNumberSInt32Type, &castWrapper) != true) {
                err = KLError_ (klMemFullErr);
            } else {
                *outNumber = castWrapper;
            }
        }
    }
    
    if (value != NULL) { CFRelease (value); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesSetNumberWithKey (const CFStringRef inKey, u_int32_t inNumber)
{
    KLStatus err = klNoErr;
    CFNumberRef value = NULL;
    SInt32 castWrapper = (SInt32) inNumber;
    
    if (inKey == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        value = CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &castWrapper);
        if (value == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesSetValue (inKey, value);
    }
    
    if (value != NULL) { CFRelease (value); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesGetBooleanWithKey (const CFStringRef inKey, KLBoolean inDefaultBoolean, KLBoolean *outBoolean)
{
    KLStatus err = klNoErr;
    CFBooleanRef value = NULL;

    if (inKey == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesCopyValue (inKey, CFBooleanGetTypeID (), (CFPropertyListRef *) &value);
    }
    
    if (err == klNoErr) {
        if (value == NULL) {
            *outBoolean = inDefaultBoolean;
        } else {
            *outBoolean = CFBooleanGetValue (value);
        }
    }
    
    if (value != NULL) { CFRelease (value); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesSetBooleanWithKey (const CFStringRef inKey, KLBoolean inBoolean)
{
    KLStatus err = klNoErr;
    CFBooleanRef value = inBoolean ? kCFBooleanTrue : kCFBooleanFalse;
    
    if (inKey == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLPreferencesSetValue (inKey, value);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean __KLPreferencesGetStringArrayWithKey (const CFStringRef inKey, const KLStringArray inDefaultStringArray, KLStringArray *outStringArray)
{
    KLStatus err = klNoErr;
    CFArrayRef value = NULL;
    
    if (inKey                == NULL) { err = KLError_ (klParameterErr); }
    if (inDefaultStringArray == NULL) { err = KLError_ (klParameterErr); }
    if (outStringArray       == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesCopyValue (inKey, CFArrayGetTypeID (), (CFPropertyListRef *) &value);
    }
    
    if (err == klNoErr) {
        if ((value == NULL) || (CFArrayGetCount (value) < 1)) {
            err = __KLCreateStringArrayFromStringArray (inDefaultStringArray, outStringArray);
        } else {
            KLStringArray stringArray = NULL;
            CFIndex valueCount = CFArrayGetCount (value);
            CFIndex i;
            
            if (err == klNoErr) {
                err = __KLCreateStringArray (&stringArray);
            }
            
            if (err == klNoErr) {
                for (i = 0; i < valueCount; i++) {
                    CFStringRef valueString = NULL;
                    char *string = NULL;
                    
                    if (err == klNoErr) {
                        valueString = (CFStringRef) CFArrayGetValueAtIndex (value, i);
                        if ((valueString == NULL) || (CFGetTypeID (valueString) != CFStringGetTypeID ())) {
                            err = KLError_ (klPreferencesReadErr);
                        }
                    }
                    
                    if (err == klNoErr) {
                        err = __KLCreateStringFromCFString (valueString, __KLApplicationGetTextEncoding(), &string);
                    }
                    
                    if (err == klNoErr) {
                        err = __KLStringArrayAddString (stringArray, string);
                    }
                    
                    if (string != NULL) { free (string); }
                }
            }
            
            if (err == klNoErr) {
                *outStringArray = stringArray;
            } else {
                if (stringArray != NULL) { __KLDisposeStringArray (stringArray); }
            }
        }
    }
    
    if (value != NULL) { CFRelease (value); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesSetStringArrayWithKey (const CFStringRef inKey, const KLStringArray inStringArray)
{
    KLStatus          err = klNoErr;
    KLIndex           arrayCount;
    CFMutableArrayRef valueArray = NULL;
    KLIndex           i;
    
    if (inKey         == NULL) { err = KLError_ (klParameterErr); }
    if (inStringArray == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLStringArrayGetStringCount (inStringArray, &arrayCount);
    }

    if (err == klNoErr) {
        valueArray = CFArrayCreateMutable (kCFAllocatorDefault, arrayCount, &kCFTypeArrayCallBacks);
    }

    if (err == klNoErr) {
        for (i = 0; i < arrayCount; i++) {
            char *string = NULL;
            CFStringRef cfString = NULL;

            if (err == klNoErr) {
                err = __KLStringArrayGetStringAtIndex (inStringArray, i, &string); // don't free
            }

            if (err == klNoErr) {
                cfString = CFStringCreateWithCString (kCFAllocatorDefault, string, __KLApplicationGetTextEncoding ());
                if (cfString == NULL) { err = KLError_ (klMemFullErr); }
            }

            if (err == klNoErr) {
                CFArrayAppendValue (valueArray, cfString);
            }

            if (cfString != NULL) { CFRelease (cfString); }  // CFArrayAppendValue will retain the string
        }
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesSetValue (inKey, valueArray);
    }
    
    if (valueArray != NULL) { CFRelease (valueArray); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLPreferencesGetLibDefaultBoolean (const char *inLibDefaultName, KLBoolean inDefaultBoolean)
{
    KLStatus     err = klNoErr;
    KLBoolean    libDefaultBoolean = inDefaultBoolean;
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *names[3] = {"libdefaults", inLibDefaultName, NULL};
    char       **values = NULL;

    if (inLibDefaultName == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        if ((profile_get_values(profile, names, &values) == 0) && (values[0] != NULL)) {
            if (strcasecmp(values[0], "y") == 0 || strcasecmp(values[0], "yes") == 0 ||
                strcasecmp(values[0], "t") == 0 || strcasecmp(values[0], "true") == 0 ||
                strcasecmp(values[0], "1") == 0 || strcasecmp(values[0], "on") == 0) {
                libDefaultBoolean = true;
            } else {
                libDefaultBoolean = false;  // In krb5, everything else is considered false
            }
        }
    }
    
    if (values  != NULL) { profile_free_list (values); }
    if (profile != NULL) { profile_abandon (profile); }
    if (context != NULL) { krb5_free_context (context); }
    
    return libDefaultBoolean;
}

// ---------------------------------------------------------------------------

static KLTime __KLPreferencesGetLibDefaultTime (const char *inLibDefaultName, KLTime inDefaultTime)
{
    KLStatus     err = klNoErr;
    KLTime       libDefaultTime = inDefaultTime;
    krb5_context context = NULL;
    profile_t    profile = NULL;
    const char  *names[3] = {"libdefaults", inLibDefaultName, NULL};
    char       **values = NULL;

    if (inLibDefaultName == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_profile (context, &profile);
    }
    
    if (err == klNoErr) {
        if ((profile_get_values(profile, names, &values) == 0) && (values[0] != NULL)) {
            err = krb5_string_to_deltat(values[0], &libDefaultTime);
        }
    }

    if (values  != NULL) { profile_free_list (values); }
    if (profile != NULL) { profile_abandon (profile); }
    if (context != NULL) { krb5_free_context (context); }
    
    return libDefaultTime;
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginName (char **outName)
{
    KLStatus err = klNoErr;
    char *osName = NULL;
    
    if (outName == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        struct passwd *pw = getpwuid (LoginSessionGetSessionUID ());
        if (pw != NULL) {
            err = __KLCreateString (pw->pw_name, &osName);
        }
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetStringWithKey (kKLName, osName ? osName : kDefaultLoginName, outName);
    }

    if (osName != NULL) { KLDisposeString (osName); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginName (const char *inName)
{
    return __KLPreferencesSetStringWithKey (kKLName, inName);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginInstance (char **outInstance)
{
    return __KLPreferencesGetStringWithKey (kKLInstance, kDefaultLoginInstance, outInstance);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginInstance (const char *inInstance)
{
	return __KLPreferencesSetStringWithKey (kKLInstance, inInstance);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginMinimumTicketLifetime (KLLifetime *outMinimumTicketLifetime)
{
	return __KLPreferencesGetNumberWithKey (kKLMinimumTicketLifetime, kDefaultMinimumTicketLifetime, outMinimumTicketLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginMinimumTicketLifetime (KLLifetime inMinimumTicketLifetime)
{
    return __KLPreferencesSetNumberWithKey (kKLMinimumTicketLifetime, inMinimumTicketLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginMaximumTicketLifetime (KLLifetime *outMaximumTicketLifetime)
{
    return __KLPreferencesGetNumberWithKey (kKLMaximumTicketLifetime, kDefaultMaximumTicketLifetime, outMaximumTicketLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginMaximumTicketLifetime (KLLifetime inMaximumTicketLifetime)
{
    return __KLPreferencesSetNumberWithKey (kKLMaximumTicketLifetime, inMaximumTicketLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultTicketLifetime (KLLifetime *outDefaultTicketLifetime)
{
    return __KLPreferencesGetNumberWithKey (kKLDefaultTicketLifetime, kDefaultTicketLifetime, outDefaultTicketLifetime);
}


// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultTicketLifetime (KLLifetime inDefaultTicketLifetime)
{
    return __KLPreferencesSetNumberWithKey (kKLDefaultTicketLifetime, inDefaultTicketLifetime);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultRenewableTicket (KLBoolean *outDefaultRenewableTicket)
{
    return __KLPreferencesGetBooleanWithKey (kKLDefaultRenewableTicket, kDefaultRenewableTicket, outDefaultRenewableTicket);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultRenewableTicket (KLBoolean inDefaultRenewableTicket)
{
    return __KLPreferencesSetBooleanWithKey (kKLDefaultRenewableTicket, inDefaultRenewableTicket);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginMinimumRenewableLifetime (KLLifetime *outMinimumRenewableLifetime)
{
	return __KLPreferencesGetNumberWithKey (kKLMinimumRenewableLifetime, kDefaultMinimumRenewableLifetime, outMinimumRenewableLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginMinimumRenewableLifetime (KLLifetime inMinimumRenewableLifetime)
{
    return __KLPreferencesSetNumberWithKey (kKLMinimumRenewableLifetime, inMinimumRenewableLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginMaximumRenewableLifetime (KLLifetime *outMaximumRenewableifetime)
{
    return __KLPreferencesGetNumberWithKey (kKLMaximumRenewableLifetime, kDefaultMaximumRenewableLifetime, outMaximumRenewableifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginMaximumRenewableLifetime (KLLifetime inMaximumRenewableLifetime)
{
	return __KLPreferencesSetNumberWithKey (kKLMaximumRenewableLifetime, inMaximumRenewableLifetime);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultRenewableLifetime (KLLifetime *outDefaultRenewableLifetime)
{
    KLTime defaultTime = __KLPreferencesGetLibDefaultTime ("renew_lifetime", kDefaultRenewableLifetime);
    
    return __KLPreferencesGetNumberWithKey (kKLDefaultRenewableLifetime, defaultTime, outDefaultRenewableLifetime);
}


// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultRenewableLifetime (KLLifetime inDefaultRenewableLifetime)
{
	return __KLPreferencesSetNumberWithKey (kKLDefaultRenewableLifetime, inDefaultRenewableLifetime);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultForwardableTicket (KLBoolean *outDefaultForwardableTicket)
{
    KLBoolean defaultForwardable = __KLPreferencesGetLibDefaultBoolean ("forwardable", kDefaultForwardableTicket);
    
    return __KLPreferencesGetBooleanWithKey (kKLDefaultForwardableTicket, defaultForwardable, outDefaultForwardableTicket);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultForwardableTicket (KLBoolean inDefaultForwardableTicket)
{
    return __KLPreferencesSetBooleanWithKey (kKLDefaultForwardableTicket, inDefaultForwardableTicket);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultProxiableTicket (KLBoolean *outDefaultProxiableTicket)
{
    KLBoolean defaultProxiable = __KLPreferencesGetLibDefaultBoolean ("proxiable", kDefaultProxiableTicket);

    return __KLPreferencesGetBooleanWithKey (kKLDefaultProxiableTicket, defaultProxiable, outDefaultProxiableTicket);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultProxiableTicket (KLBoolean inDefaultProxiableTicket)
{
    return __KLPreferencesSetBooleanWithKey (kKLDefaultProxiableTicket, inDefaultProxiableTicket);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultAddresslessTicket (KLBoolean *outAddresslessTicket)
{
    KLBoolean defaultAddressless = __KLPreferencesGetLibDefaultBoolean ("noaddresses", kDefaultAddresslessTicket);

    return __KLPreferencesGetBooleanWithKey (kKLDefaultAddresslessTicket, defaultAddressless, outAddresslessTicket);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultAddresslessTicket (KLBoolean inDefaultAddresslessTicket)
{
    return __KLPreferencesSetBooleanWithKey (kKLDefaultAddresslessTicket, inDefaultAddresslessTicket);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginShowOptions (KLBoolean *outShowOptions)
{
    return __KLPreferencesGetBooleanWithKey (kKLShowOptions, kDefaultShowOptions, outShowOptions);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginShowOptions (KLBoolean inShowOptions)
{
    return __KLPreferencesSetBooleanWithKey (kKLShowOptions, inShowOptions);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginLongLifetimeDisplay (KLBoolean *outLongLifetimeDisplay)
{
    return __KLPreferencesGetBooleanWithKey (kKLLongLifetimeDisplay, kDefaultLongLifetimeDisplay, outLongLifetimeDisplay);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginLongLifetimeDisplay (KLBoolean inLongTimeDisplay)
{
    return __KLPreferencesSetBooleanWithKey (kKLLongLifetimeDisplay, inLongTimeDisplay);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginRememberShowOptions (KLBoolean *outRememberShowOptions)
{
    return __KLPreferencesGetBooleanWithKey (kKLRememberShowOptions, kDefaultRememberShowOptions, outRememberShowOptions);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginRememberShowOptions (KLBoolean inRememberShowOptions)
{
    return __KLPreferencesSetBooleanWithKey (kKLRememberShowOptions, inRememberShowOptions);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginRememberPrincipal (KLBoolean *outRememberPrincipal)
{
	return __KLPreferencesGetBooleanWithKey (kKLRememberPrincipal, kDefaultRememberPrincipal, outRememberPrincipal);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginRememberPrincipal (KLBoolean inRememberPrincipal)
{
    return __KLPreferencesSetBooleanWithKey (kKLRememberPrincipal, inRememberPrincipal);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginRememberExtras (KLBoolean *outRememberExtras)
{
	return __KLPreferencesGetBooleanWithKey (kKLRememberExtras, kDefaultRememberExtras, outRememberExtras);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginRememberExtras (KLBoolean inRememberExtras)
{
    return __KLPreferencesSetBooleanWithKey (kKLRememberExtras, inRememberExtras);
}

#pragma mark -

// ---------------------------------------------------------------------------

/*
 * Realm configuration
 */
 

// ---------------------------------------------------------------------------

static KLStatus __KLPreferencesGetFavoriteRealmList (KLStringArray *outRealmList)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    
    if (err == klNoErr) {
        err = __KLPreferencesGetStringArrayWithKey (kKLFavoriteRealms, kDefaultRealmList, &realmList);
    }
    
    if (err == klNoErr) {
    	err = __KLPreferencesEnsureKerberosDefaultRealmIsInFavorites (realmList);
    }
    
    if (err == klNoErr) {
        *outRealmList = realmList;
    } else {
        __KLDisposeStringArray (realmList);
    }
    
    return KLError_ (err);
}

static KLStatus __KLPreferencesSetFavoriteRealmList (KLStringArray inRealmList)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;

    if (err == klNoErr) {
        err = __KLCreateStringArrayFromStringArray (inRealmList, &realmList);  // So we don't muck with the input
    }

    if (err == klNoErr) {
    	err = __KLPreferencesEnsureKerberosDefaultRealmIsInFavorites (realmList);
    }

    if (err == klNoErr) {
        err = __KLPreferencesSetStringArrayWithKey (kKLFavoriteRealms, realmList);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

static KLStatus __KLPreferencesGetKerberosDefaultRealm (char **outDefaultRealm)
{
    KLStatus     err = klNoErr;
    krb5_context context;
    const char  *defaultRealm = NULL;
    char        *defaultRealmV5 = NULL;
    char         defaultRealmV4[REALM_SZ];
	
    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }
    
    if (err == klNoErr) {
    	if (krb5_get_default_realm(context, &defaultRealmV5) == 0) {
            defaultRealm = defaultRealmV5;
        } else if (krb_get_lrealm (defaultRealmV4, 1) == KSUCCESS) {
            defaultRealm = defaultRealmV4;
        } else {
            defaultRealm = kNoDefaultRealm;
        }
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (defaultRealm, outDefaultRealm);
    }
    
    if (defaultRealmV5 != NULL) { krb5_free_default_realm (context, defaultRealmV5); }
    if (context        != NULL) { krb5_free_context (context); }
    
    return KLError_ (err);
}

/* Make sure that the default realm is part of the favorite realms. */

static KLStatus __KLPreferencesEnsureKerberosDefaultRealmIsInFavorites (KLStringArray inRealmList)
{
    KLStatus err = klNoErr;
    char *defaultRealm = NULL;
    char *defaultKerberosRealm = NULL;
    KLIndex index;

    if (inRealmList == NULL) { err = KLError_ (klParameterErr); }
    
    // Make sure the Kerberos default realm is in the KLL favorite realms list
    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosDefaultRealm (&defaultKerberosRealm);
    }

    if (err == klNoErr) {
        if (defaultKerberosRealm[0] != '\0') {  // Not an empty realm
            if (__KLStringArrayGetIndexForString (inRealmList, defaultKerberosRealm, &index) != klNoErr) {
                // realm not present... Add it.
                err = __KLStringArrayInsertStringBeforeIndex (inRealmList, defaultKerberosRealm, 0);
            }
        }
    }
    
    // Make sure the KLL default realm is in the realms list
    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosLoginDefaultRealmByName (&defaultRealm);
        
    }

    if (err == klNoErr) {
        if (defaultRealm[0] != '\0') {  // Not an empty realm
            if (__KLStringArrayGetIndexForString (inRealmList, defaultRealm, &index) != klNoErr) {
                // realm not present and no other realms in the list... Add it.
                err = __KLStringArrayInsertStringBeforeIndex (inRealmList, defaultRealm, 0);
            }
        }
    }
    
    if (defaultKerberosRealm != NULL) { KLDisposeString (defaultKerberosRealm); }
    if (defaultRealm         != NULL) { KLDisposeString (defaultRealm); }

    return KLError_ (err);
}

#pragma mark -

KLStatus __KLPreferencesGetKerberosLoginRealm (KLIndex inIndex, char **outRealm)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    
    if (outRealm == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayCopyStringAtIndex (realmList, inIndex, outRealm);
        if (err != klNoErr) { err = KLError_ (klRealmDoesNotExistErr); }
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginRealmByName (const char *inName, KLIndex *outIndex)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    
    if (inName   == NULL) { err = KLError_ (klParameterErr); }
    if (outIndex == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayGetIndexForString (realmList, inName, outIndex);
        if (err != klNoErr) { err = KLError_ (klRealmDoesNotExistErr); }
    }

    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginRealm (KLIndex inIndex, const char *inName)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    KLIndex count, i;
    KLIndex index = inIndex;
    
    if (inName == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayGetStringCount (realmList, &count);
    }
    
    if (err == klNoErr) {
        err = __KLStringArraySetStringAtIndex (realmList, inName, index);
        if (err != klNoErr) { err = KLError_ (klRealmDoesNotExistErr); }
    }
    
    // Find duplicates before the new realm and remove them
    if (err == klNoErr) {
        for (i = 0; i < index; i++) {
            char *string = NULL;
            
            if (err == klNoErr) {
                err = __KLStringArrayGetStringAtIndex (realmList, i, &string); // don't free
            }
            
            if (err == klNoErr) {
                if (strcmp (string, inName) == 0) {
                    err = __KLStringArrayRemoveStringAtIndex (realmList, i);
                    if (err == klNoErr) { i--; index--; }
                }
            }
        }
    }

    // Find duplicates after the new realm and remove them
    if (err == klNoErr) {
        for (i = index + 1; i < count; i++) {
            char *string = NULL;
            
            if (err == klNoErr) {
                err = __KLStringArrayGetStringAtIndex (realmList, i, &string); // don't free
            }
            
            if (err == klNoErr) {
                if (strcmp (string, inName) == 0) {
                    err = __KLStringArrayRemoveStringAtIndex (realmList, i);
                    if (err == klNoErr) { i--; }
                }
            }
        }
    }    

    if (err == klNoErr) {
        err = __KLPreferencesSetFavoriteRealmList (realmList);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesRemoveKerberosLoginRealm (KLIndex inIndex)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayRemoveStringAtIndex (realmList, inIndex);
        if (err != klNoErr) { err = KLError_ (klRealmDoesNotExistErr); }
    }

    if (err == klNoErr) {
        err = __KLPreferencesSetFavoriteRealmList (realmList);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesInsertKerberosLoginRealm (KLIndex inIndex, const char *inName)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    KLIndex index = 0;
    KLIndex count, i;
    
    if (inName == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayGetStringCount (realmList, &count);
    }

    if (err == klNoErr) {
        index = (inIndex > count) ? count : inIndex;  // make sure the index isn't greater than the count
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayInsertStringBeforeIndex (realmList, inName, index);
    }

    // Find duplicates before the new realm and remove them
    if (err == klNoErr) {
        for (i = 0; i < index; i++) {
            char *string = NULL;
            
            if (err == klNoErr) {
                err = __KLStringArrayGetStringAtIndex (realmList, i, &string); // don't free
            }
            
            if (err == klNoErr) {
                if (strcmp (string, inName) == 0) {
                    err = __KLStringArrayRemoveStringAtIndex (realmList, i);
                    if (err == klNoErr) { i--; index--; }
                }
            }
        }
    }

    // Find duplicates after the new realm and remove them
    if (err == klNoErr) {
        for (i = index + 1; i < count; i++) {
            char *string = NULL;
            
            if (err == klNoErr) {
                err = __KLStringArrayGetStringAtIndex (realmList, i, &string); // don't free
            }
            
            if (err == klNoErr) {
                if (strcmp (string, inName) == 0) {
                    err = __KLStringArrayRemoveStringAtIndex (realmList, i);
                    if (err == klNoErr) { i--; }
                }
            }
        }
    }    

    if (err == klNoErr) {
        err = __KLPreferencesSetFavoriteRealmList (realmList);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesRemoveAllKerberosLoginRealms (void)
{
    KLStatus err = klNoErr;

    if (err == klNoErr) {
        err = __KLPreferencesSetKerberosLoginDefaultRealmByName (kNoDefaultRealm);
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesSetFavoriteRealmList (kKLEmptyStringArray);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesCountKerberosLoginRealms (KLIndex *outIndex)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    
    if (outIndex == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        err = __KLStringArrayGetStringCount (realmList, outIndex);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultRealm (KLIndex *outIndex)
{
    KLStatus err = klNoErr;
    char *defaultRealm = NULL;
    
    if (outIndex == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosLoginDefaultRealmByName (&defaultRealm);
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosLoginRealmByName (defaultRealm, outIndex);
    }
    
    if (defaultRealm != NULL) { KLDisposeString (defaultRealm); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesGetKerberosLoginDefaultRealmByName (char **outDefaultRealm)
{
    KLStatus err = klNoErr;
    char *kerberosDefaultRealm = NULL;
    
    if (outDefaultRealm == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosDefaultRealm (&kerberosDefaultRealm);
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesGetStringWithKey (kKLDefaultRealm, kerberosDefaultRealm, outDefaultRealm);
    }
    
    if (kerberosDefaultRealm != NULL) { KLDisposeString (kerberosDefaultRealm); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultRealm (KLIndex inIndex)
{
    KLStatus err = klNoErr;
    char *newDefaultRealm = NULL;
    
    if (err == klNoErr) {
        err = __KLPreferencesGetKerberosLoginRealm (inIndex, &newDefaultRealm);
    }
    
    if (err == klNoErr) {
        err = __KLPreferencesSetStringWithKey (kKLDefaultRealm, newDefaultRealm);
    }
    
    if (newDefaultRealm != NULL) { KLDisposeString (newDefaultRealm); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLPreferencesSetKerberosLoginDefaultRealmByName (const char *inName)
{
    KLStatus err = klNoErr;
    KLStringArray realmList = NULL;
    KLIndex index;
    
    if (err == klNoErr) {
        err = __KLPreferencesGetFavoriteRealmList (&realmList);
    }
    
    if (err == klNoErr) {
        if (inName[0] != '\0') { // Allow empty default realm
            err = __KLStringArrayGetIndexForString (realmList, inName, &index);
            if (err != klNoErr) { err = KLError_ (klRealmDoesNotExistErr); }
        }
    }

    if (err == klNoErr) {
        err = __KLPreferencesSetStringWithKey (kKLDefaultRealm, inName);
    }
    
    if (realmList != NULL) { __KLDisposeStringArray (realmList); }
    
    return KLError_ (err);
}
