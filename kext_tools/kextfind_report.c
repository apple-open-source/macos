/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include "kextfind.h"
#include "kextfind_report.h"
#include "kextfind_query.h"
#include "kextfind_commands.h"
#include "utility.h"

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/fat_util.h>
#include <IOKit/kext/macho_util.h>

/*******************************************************************************
* External function prototypes.
*
* XXX: These should be exported in private headers.
*******************************************************************************/
// XXX: Put vers_rsrc.h in private headers export for IOKitUser
typedef SInt64 VERS_version;
extern VERS_version _KXKextGetCompatibleVersion(KXKextRef aKext);
extern fat_iterator _KXKextCopyFatIterator(KXKextRef aKext);

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseProperty(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    uint32_t index        = 1;

   /* Fudge the predicate so we can use one eval callback.
    */
    QEQueryElementSetPredicate(element, CFSTR(kPredNameProperty));

   /* Parse the property name to retrieve.
   */
    if (!parseArgument(element, &argv[index], &index, user_data, error)) {
        goto finish;
    }

    result = true;
finish:
    *num_used += index;
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseShorthand(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    CFStringRef predicate = QEQueryElementGetPredicate(element);
    uint32_t index = 1;

    if (CFEqual(predicate, CFSTR(kPredNameBundleID))) {
        QEQueryElementAppendArgument(element, kCFBundleIdentifierKey);
    } else if (CFEqual(predicate, CFSTR(kPredNameBundleName))) {
        QEQueryElementAppendArgument(element, kCFBundleNameKey);
    } else if (CFEqual(predicate, CFSTR(kPredNameVersion))) {
        QEQueryElementAppendArgument(element, kCFBundleVersionKey);
    } else {
        goto finish;
    }

    QEQueryElementSetPredicate(element, CFSTR(kPredNameProperty));

    result = true;
finish:
    *num_used += index;
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
char * cStringForCFValue(CFTypeRef value)
{
    CFTypeID valueType;
    CFIndex count;
    char buffer[80];  // more than big enough for a number

    if (!value) {
        return strdup("<null>");
    }

    valueType = CFGetTypeID(value);

    if (CFStringGetTypeID() == valueType) {
        return cStringForCFString(value);
    } else if (CFBooleanGetTypeID() == valueType) {
        return CFBooleanGetValue(value) ? strdup(kWordTrue) : strdup(kWordFalse);
    } else if (CFNumberGetTypeID() == valueType) {
    } else if (CFArrayGetTypeID() == valueType) {
        count = CFArrayGetCount(value);
        snprintf(buffer, (sizeof(buffer)/sizeof(char)), "<array of %ld>", count);
        return strdup(buffer);
    } else if (CFDictionaryGetTypeID() == valueType) {
        count = CFDictionaryGetCount(value);
        snprintf(buffer, (sizeof(buffer)/sizeof(char)), "<dict of %ld>", count);
        return strdup(buffer);
    } else if (CFDataGetTypeID() == valueType) {
        count = CFDataGetLength(value);
        snprintf(buffer, (sizeof(buffer)/sizeof(char)), "<data of %ld>", count);
        return strdup(buffer);
    } else {
        return strdup("<unknown CF type>");
    }
    return NULL;
}

/*******************************************************************************
* Note: reportEvalCommand() calls this.
*******************************************************************************/
Boolean reportEvalProperty(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    KXKextRef theKext = (KXKextRef)object;
    QueryContext * context = (QueryContext *)user_data;
    CFStringRef propKey = NULL;   // don't release
    CFTypeRef   propVal = NULL;   // don't release
    char *      cString = NULL;   // must free

    propKey = QEQueryElementGetArgumentAtIndex(element, 0);
    if (!propKey) {
        *error = kQEQueryErrorEvaluationCallbackFailed;
        goto finish;
    }

    if (!context->reportStarted) {
        cString = cStringForCFString(propKey);
        if (!cString) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        printf("%s%s", context->reportRowStarted ? "\t" : "",
            cString);
    } else {
        // This is allowed to be null
        propVal = CFDictionaryGetValue(KXKextGetInfoDictionary(theKext), propKey);
        cString = cStringForCFValue(propVal);
        if (!cString) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        printf("%s%s", context->reportRowStarted ? "\t" : "",
            cString);
    }

    context->reportRowStarted = true;

    result = true;
finish:
    if (cString) free(cString);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseFlag(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    CFStringRef flag = QEQueryElementGetPredicate(element);
    QueryContext * context = (QueryContext *)user_data;
    uint32_t index = 1;

    QEQueryElementSetPredicate(element, CFSTR(kPredNameFlag));
    CFDictionarySetValue(element, CFSTR(kKeywordFlag), flag);

    if (CFEqual(flag, CFSTR(kPredNameLoaded))) {
        context->checkLoaded = true;
    } else if (CFEqual(flag, CFSTR(kPredNameIntegrity))) {
        context->checkIntegrity = true;
    } else if (CFEqual(flag, CFSTR(kPredNameAuthentic)) ||
        CFEqual(flag, CFSTR(kPredNameInauthentic))) {
        context->checkAuthentic = true;
    } else if (CFEqual(flag, CFSTR(kPredNameLoadable)) ||
        CFEqual(flag, CFSTR(kPredNameNonloadable))) {
        context->checkAuthentic = true;
        context->checkLoadable = true;
    }

    result = true;

    *num_used += index;
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportEvalFlag(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    KXKextRef theKext = (KXKextRef)object;
    CFStringRef flag = CFDictionaryGetValue(element, CFSTR(kKeywordFlag));
    QueryContext * context = (QueryContext *)user_data;
    char *         cString = NULL;   // don't free!
    Boolean print = true;

    if (!context->reportStarted) {
        if (CFEqual(flag, CFSTR(kPredNameLoaded))) {
            cString = "Loaded";
        } else if (CFEqual(flag, CFSTR(kPredNameValid))) {
            cString = "Valid";
        } else if (CFEqual(flag, CFSTR(kPredNameAuthentic))) {
            cString = "Authentic";
        } else if (CFEqual(flag, CFSTR(kPredNameDependenciesMet))) {
            cString = "Dependencies Met";
        } else if (CFEqual(flag, CFSTR(kPredNameLoadable))) {
            cString = "Loadable";
        } else if (CFEqual(flag, CFSTR(kPredNameWarnings))) {
            cString = "Warnings";
        } else if (CFEqual(flag, CFSTR(kPredNameIsLibrary))) {
            cString = "Library";
        } else if (CFEqual(flag, CFSTR(kPredNameHasPlugins))) {
            cString = "Plugins";
        } else if (CFEqual(flag, CFSTR(kPredNameIsPlugin))) {
            cString = "Is Plugin";
        } else if (CFEqual(flag, CFSTR(kPredNameHasDebugProperties))) {
            cString = "Debug";
        } else if (CFEqual(flag, CFSTR(kPredNameIsKernelResource))) {
            cString = "Kernel Resource";
        } else if (CFEqual(flag, CFSTR(kPredNameIntegrity))) {
            cString = "Integrity";
        } else if (CFEqual(flag, CFSTR(kPredNameExecutable))) {
            cString = "Has Executable";
        } else {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }

        printf("%s%s", context->reportRowStarted ? "\t" : "", cString);
    } else {

        if (CFEqual(flag, CFSTR(kPredNameLoaded))) {
            cString = KXKextIsLoaded(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameValid))) {
            cString = KXKextIsValid(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameAuthentic))) {
            cString = KXKextIsAuthentic(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameDependenciesMet))) {
            cString = KXKextGetHasAllDependencies(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameLoadable))) {
            cString = KXKextIsLoadable(theKext, false /* safe boot */) ?
                kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameWarnings))) {
            CFDictionaryRef warnings = KXKextGetWarnings(theKext);
            cString = (warnings && CFDictionaryGetCount(warnings)) ?
                kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameIsLibrary))) {
            cString = (_KXKextGetCompatibleVersion(theKext) > 0) ?
                kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameHasPlugins))) {
            CFArrayRef plugins = KXKextGetPlugins(theKext);
            cString = (plugins && CFArrayGetCount(plugins)) ?
                kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameIsPlugin))) {
            cString = KXKextIsAPlugin(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameHasDebugProperties))) {
            cString = KXKextHasDebugProperties(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameIsKernelResource))) {
            cString = KXKextGetIsKernelResource(theKext) ? kWordYes : kWordNo;
        } else if (CFEqual(flag, CFSTR(kPredNameIntegrity))) {
            printf("%s%s", context->reportRowStarted ? "\t" : "",
                nameForIntegrityState(KXKextGetIntegrityState(theKext)));
            print = false;
        } else if (CFEqual(flag, CFSTR(kPredNameExecutable))) {
            cString = KXKextGetDeclaresExecutable(theKext) ? kWordYes : kWordNo;
        }

        if (print) {
            if (!cString) {
                *error = kQEQueryErrorEvaluationCallbackFailed;
                goto finish;
            }
            printf("%s%s", context->reportRowStarted ? "\t" : "",
                cString);
        }
    }

    context->reportRowStarted = true;

    result = true;
finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseArch(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;

    if (!argv[(*num_used) + 1]) {
        goto finish;
    }

    CFDictionarySetValue(element, CFSTR("label"),
        createCFString(argv[(*num_used) + 1]));

    result = parseArch(element, argc, argv, num_used, user_data, error);
    if (!result) {
        goto finish;
    }

    result = true;
finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportEvalArch(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    QueryContext * context = (QueryContext *)user_data;
    CFStringRef string = NULL;   // don't release
    char *      cString = NULL;   // must free


    if (!context->reportStarted) {
        string = CFDictionaryGetValue(element, CFSTR("label"));
        if (!string) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        cString = cStringForCFString(string);
        if (!cString) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        printf("%s%s", context->reportRowStarted ? "\t" : "",
            cString);
    } else {
        Boolean match = evalArch(element, object, user_data, error);
        if (*error != kQEQueryErrorNone) {
            goto finish;
        }
        printf("%s%s", context->reportRowStarted ? "\t" : "",
            match ? kWordYes : kWordNo);
    }

    context->reportRowStarted = true;

    result = true;
finish:
    if (cString) free(cString);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportEvalArchExact(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    QueryContext * context = (QueryContext *)user_data;
    CFStringRef string = NULL;   // don't release
    char *      cString = NULL;   // must free


    if (!context->reportStarted) {
        string = CFDictionaryGetValue(element, CFSTR("label"));
        if (!string) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        cString = cStringForCFString(string);
        if (!cString) {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }
        printf("%s%s (only)", context->reportRowStarted ? "\t" : "",
            cString);
    } else {
        Boolean match = evalArchExact(element, object, user_data, error);
        if (*error != kQEQueryErrorNone) {
            goto finish;
        }
        printf("%s%s", context->reportRowStarted ? "\t" : "",
            match ? kWordYes : kWordNo);
    }

    context->reportRowStarted = true;

    result = true;
finish:
    if (cString) free(cString);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseDefinesOrReferencesSymbol(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    return parseDefinesOrReferencesSymbol(element, argc, argv, num_used,
        user_data, error);
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportEvalDefinesOrReferencesSymbol(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    KXKextRef  theKext = (KXKextRef)object;
    QueryContext * context = (QueryContext *)user_data;
    CFStringRef symbol = QEQueryElementGetArgumentAtIndex(element, 0);
    char * cSymbol = NULL;   // must free
    const char * value = "";  // don't free
    fat_iterator fiter = NULL;  // must close
    struct mach_header * farch = NULL;
    void * farch_end = NULL;
    const struct nlist * symtab_entry = NULL;

    if (!symbol) {
        *error = kQEQueryErrorEvaluationCallbackFailed;
        goto finish;
    }
    cSymbol = cStringForCFString(symbol);
    if (!cSymbol) {
        *error = kQEQueryErrorEvaluationCallbackFailed;
        goto finish;
    }

    if (!context->reportStarted) {
        printf("%ssymbol %s", context->reportRowStarted ? "\t" : "",
            cSymbol);
    } else {

        fiter = _KXKextCopyFatIterator(theKext);
        if (!fiter) {
            goto finish;
        }

        while ((farch = fat_iterator_next_arch(fiter, &farch_end))) {
            macho_seek_result seek_result = macho_find_symbol(
                farch, farch_end, cSymbol, &symtab_entry, NULL);

            if (seek_result == macho_seek_result_found_no_value ||
                seek_result == macho_seek_result_found) {

                if ((N_TYPE & symtab_entry->n_type) == N_UNDF) {
                    value = KXKextGetIsKernelResource(theKext) ?
                        "defines" : "references";
                } else {
                    value = "defines";
                }
                result = true;
                break;
            }
        }
    }

    printf("%s%s", context->reportRowStarted ? "\t" : "", value);
    context->reportRowStarted = true;

    result = true;
finish:
    if (cSymbol) free(cSymbol);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportParseCommand(
    CFMutableDictionaryRef element,
    int argc,
    char * const argv[],
    uint32_t * num_used,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    QueryContext * context = (QueryContext *)user_data;
    CFStringRef command = QEQueryElementGetPredicate(element);
    uint32_t index = 1;

    if (CFEqual(command, CFSTR(kPredNamePrintProperty))) {

       /* Fudge the predicate so we can use one eval callback.
        */
        QEQueryElementSetPredicate(element, CFSTR(kPredNameProperty));
        if (!parseArgument(element, &argv[index], &index, user_data, error)) {
            goto finish;
        }
    } else if (CFEqual(command, CFSTR(kPredNamePrintIntegrity))) {
        context->checkIntegrity = true;
    }

    CFDictionarySetValue(element, CFSTR(kKeywordCommand), command);
    QEQueryElementSetPredicate(element, CFSTR(kPredNameCommand));

    result = true;
finish:
    *num_used += index;
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean reportEvalCommand(
    CFDictionaryRef element,
    void * object,
    void * user_data,
    QEQueryError * error)
{
    Boolean result = false;
    CFStringRef command = CFDictionaryGetValue(element, CFSTR(kKeywordCommand));
    KXKextRef theKext = (KXKextRef)object;
    QueryContext * context = (QueryContext *)user_data;
    char * cString = NULL; // must free

    // if we do arches, easier to print than generate a string
    Boolean print = true;

    CFArrayRef dependencies = NULL; // must release
    CFArrayRef dependents = NULL;   // must release
    CFArrayRef plugins = NULL;          // do NOT release
    CFIndex count;
    char buffer[80];   // more than enough for an int

    if (!context->reportStarted) {

        if (CFEqual(command, CFSTR(kPredNamePrint)) ||
            CFEqual(command, CFSTR(kPredNameBundleName))) {
            cString = strdup("Bundle");
        } else if (CFEqual(command, CFSTR(kPredNamePrintProperty))) {
            result = reportEvalProperty(element, object, user_data, error);
            goto finish;
        } else if (CFEqual(command, CFSTR(kPredNamePrintArches))) {
            cString = strdup("Arches");
        } else if (CFEqual(command, CFSTR(kPredNamePrintDependencies))) {
            cString = strdup("# Dependencies");
        } else if (CFEqual(command, CFSTR(kPredNamePrintDependents))) {
            cString = strdup("# Dependents");
        } else if (CFEqual(command, CFSTR(kPredNamePrintPlugins))) {
            cString = strdup("# Plugins");
        } else if (CFEqual(command, CFSTR(kPredNamePrintIntegrity))) {
            cString = strdup("Integrity");
        } else if (CFEqual(command, CFSTR(kPredNamePrintInfoDictionary))) {
            cString = strdup("Info Dictionary");
        } else if (CFEqual(command, CFSTR(kPredNamePrintExecutable))) {
            cString = strdup("Executable");
        } else {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }

        printf("%s%s", context->reportRowStarted ? "\t" : "", cString);
    } else {
        if (CFEqual(command, CFSTR(kPredNamePrint))) {
            cString = getKextPath(theKext, context->pathSpec);
        } else if (CFEqual(command, CFSTR(kPredNameBundleName))) {
            cString = getKextPath(theKext, kPathsNone);
        } else if (CFEqual(command, CFSTR(kPredNamePrintProperty))) {
            result = reportEvalProperty(element, object, user_data, error);
            goto finish;
        } else if (CFEqual(command, CFSTR(kPredNamePrintArches))) {
            printf("%s", context->reportRowStarted ? "\t" : "");
            printKextArches(theKext, 0, false /* print line end */);
            print = false;
        } else if (CFEqual(command, CFSTR(kPredNamePrintDependencies))) {
            dependencies = KXKextCopyAllDependencies(theKext);
            count = dependencies ? CFArrayGetCount(dependencies) : 0;
            snprintf(buffer, (sizeof(buffer)/sizeof(char)), "%ld", count);
            cString = strdup(buffer);
        } else if (CFEqual(command, CFSTR(kPredNamePrintDependents))) {
            dependencies = KXKextCopyAllDependents(theKext);
            count = dependencies ? CFArrayGetCount(dependencies) : 0;
            snprintf(buffer, (sizeof(buffer)/sizeof(char)), "%ld", count);
            cString = strdup(buffer);
        } else if (CFEqual(command, CFSTR(kPredNamePrintPlugins))) {
            plugins = KXKextGetPlugins(theKext);
            count = plugins ? CFArrayGetCount(plugins) : 0;
            snprintf(buffer, (sizeof(buffer)/sizeof(char)), "%ld", count);
            cString = strdup(buffer);
        } else if (CFEqual(command, CFSTR(kPredNamePrintIntegrity))) {
            printf("%s%s", context->reportRowStarted ? "\t" : "",
                nameForIntegrityState(KXKextGetIntegrityState(theKext)));
            print = false;
        } else if (CFEqual(command, CFSTR(kPredNamePrintInfoDictionary))) {
            cString = getKextInfoDictionaryPath(theKext, context->pathSpec);
            if (!cString) {
                cString = strdup("");
            }
        } else if (CFEqual(command, CFSTR(kPredNamePrintExecutable))) {
            cString = getKextExecutablePath(theKext, context->pathSpec);
            if (!cString) {
                cString = strdup("");
            }
        } else {
            *error = kQEQueryErrorEvaluationCallbackFailed;
            goto finish;
        }

        if (print) {
            if (!cString) {
                *error = kQEQueryErrorEvaluationCallbackFailed;
                goto finish;
            }
            printf("%s%s", context->reportRowStarted ? "\t" : "",
                cString);
        }
    }

    context->reportRowStarted = true;
    result = true;
finish:
    if (cString)      free(cString);
    if (dependencies) CFRelease(dependencies);
    if (dependents)   CFRelease(dependents);
    return result;
}
