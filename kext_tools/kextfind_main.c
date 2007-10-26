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
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/kext/KXKextManager.h>

#include "kextfind.h"
#include "kextfind_tables.h"
#include "kextfind_query.h"
#include "kextfind_commands.h"
#include "kextfind_report.h"
#include "utility.h"
#include "QEQuery.h"

/*******************************************************************************
* Misc. macros.
*******************************************************************************/

#define kKextSuffix            ".kext"


/*******************************************************************************
* Function prototypes.
*******************************************************************************/
Boolean addSearchItem(
    const char * optarg,
    CFMutableArrayRef repositoryDirectories,
    CFMutableArrayRef kextNames);

static int allocateArray(CFMutableArrayRef * array);

static void usage(int level);

/*******************************************************************************
* Global variables (non-static referenced by utility.c).
*******************************************************************************/

const char * progname = "(unknown)";
int g_verbose_level = kKXKextManagerLogLevelDefault;  // for utility.c

/*******************************************************************************
*
*******************************************************************************/
int main(int argc, char * const *argv)
{
    int exit_code = 0;

    int opt_char = 0;
    int last_optind;  // for recovering from getopt failures

    CFIndex count, i;

    QEQueryRef query = NULL;
    struct querySetup * queryCallback = queryCallbackList;

    QEQueryRef reportQuery = NULL;
    struct querySetup * reportCallback = reportCallbackList;
    uint32_t reportStartIndex;

    QueryContext queryContext = { 0, };
    Boolean queryStarted = false;
    uint32_t numArgsUsed = 0;

    CFMutableArrayRef repositoryDirectories = NULL;  // must release
    CFMutableArrayRef kextNames = NULL;              // must release

    KXKextManagerRef theKextManager = NULL;          // must release
    KXKextRef theKext = NULL;                        // don't release
    KXKextManagerError kmErr;
    int addKextsResult = 1;  // assume success here

    CFArrayRef        candidateKexts = NULL;         // must release

    queryContext.assertiveness = kKextfindPicky;

   /*****
    * Find out what the program was invoked as.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

   /*****
    * Allocate collection objects needed for command line argument processing.
    */
    if (!allocateArray(&repositoryDirectories)) {
        goto finish;
    }

    if (!allocateArray(&kextNames)) {
        goto finish;
    }

   /*****
    * Process command-line arguments.
    */
    opterr = 0;
    last_optind = optind;
    while ((opt_char = getopt_long_only(argc, argv, kOPT_CHARS,
        opt_info, NULL)) != -1) {

        switch (opt_char) {

          case kOptHelp:
            usage(2);
            exit_code = 0;
            goto finish;
            break;

          case kOptCaseInsensitive:
            queryContext.caseInsensitive = true;
            break;

          case kOptSearchItem:
            if (!check_dir(optarg, 0 /* writeable */, 0 /* print error */)) {
                qerror("%s is not a kext or directory\n", optarg);
                goto finish;
            }

            if (!addSearchItem(optarg, repositoryDirectories, kextNames)) {
                goto finish;
            };
            break;

          case kOptSubstring:
            queryContext.substrings = true;
            break;

          case kOptSystemExtensions:
            CFArrayAppendValue(repositoryDirectories, kKXSystemExtensionsFolder);
            break;

          case 0:
            switch (longopt) {

              case kLongOptQueryPredicate:
                optind = last_optind;
                if (argv[last_optind] && (argv[last_optind][0] != '-')) {
                    // probably a directory; stupid getopt_long_only()
                    goto options_finished;
                }
                goto begin_query_or_report;
                break;

#ifdef EXTRA_INFO
              case kLongOptExtraInfo:
                queryContext.extraInfo = true;
                break;
#endif
              case kLongOptRelativePaths:
               /* Last one specified wins! */
                queryContext.pathSpec = kPathsRelative;
                break;

              case kLongOptNoPaths:
               /* Last one specified wins! */
                queryContext.pathSpec = kPathsNone;
                break;

#ifdef MEEK_PICKY
              case kLongOptMeek:
                queryContext.assertiveness = kKextfindMeek;
                break;

              case kLongOptPicky:
                queryContext.assertiveness = kKextfindPicky;
                break;
#endif

              default:
                qerror("internal argument processing error\n");
                goto finish;
                break;

            }
            longopt = 0;
            break;

          default:
           /* getopt_long() gives us '?' if we turn off errors, so just
            * move on to query parsing. Sometimes optind jumps ahead too
            * far, so we restore it to the value before the call to
            * getopt_long_only() -- we can't just decrement it.
            */
            optind = last_optind;
            goto options_finished;
            break;
        }

        last_optind = optind;
    }

/*****
 * Check the arguments following the options to see if they are
 * potential search directories or kexts.
 */
options_finished:

    while (check_dir(argv[optind], 0 /* writeable */, 0 /* print error */)) {
        if (!addSearchItem(argv[optind], repositoryDirectories, kextNames)) {
            goto finish;
        };
        optind++;
    }

/*****
 * Set up the query.
 */
begin_query_or_report:

   /****************************************
    */
    query = QEQueryCreate(&queryContext);
    if (!query) {
        fprintf(stderr, "Can't create query\n");
        goto finish;
    }

    while (queryCallback->longName) {
        if (queryCallback->parseCallback) {
            QEQuerySetParseCallbackForPredicate(query, queryCallback->longName,
                queryCallback->parseCallback);
            if (queryCallback->shortName) {
                QEQuerySetSynonymForPredicate(query, queryCallback->shortName,
                    queryCallback->longName);
            }
        }
        if (queryCallback->evalCallback) {
            QEQuerySetEvaluationCallbackForPredicate(query,
                queryCallback->longName,
                queryCallback->evalCallback);
        }
        queryCallback++;
    }
    QEQuerySetSynonymForPredicate(query, CFSTR("!"), CFSTR(kQEQueryTokenNot));

    numArgsUsed = optind;

   /* If we're not immediately doing a report spec, parse the query.
    */
    if (argv[numArgsUsed] && strcmp(argv[numArgsUsed], kKeywordReport)) {
        while (QEQueryAppendElementFromArgs(query, argc - numArgsUsed,
            &argv[numArgsUsed], &numArgsUsed)) {

            queryStarted = true;
            if (argv[numArgsUsed] && !strcmp(argv[numArgsUsed], kKeywordReport)) {
                break;
            }
        }

    }

    if (QEQueryLastError(query) != kQEQueryErrorNone) {
        switch (QEQueryLastError(query)) {
          case kQEQueryErrorNoMemory:
            fprintf(stderr, "memory allocation failure\n");
            break;
          case kQEQueryErrorEmptyGroup:
            fprintf(stderr, "empty group near arg #%d\n", numArgsUsed);
            break;
          case kQEQueryErrorSyntax:
            fprintf(stderr, "query syntax error near '%s' (arg #%d)\n",
                argv[numArgsUsed], numArgsUsed);
            break;
          case kQEQueryErrorNoParseCallback:
            if (queryStarted) {
                fprintf(stderr, "expected query predicate, found '%s' (arg #%d)\n",
                    argv[numArgsUsed], numArgsUsed);
            } else {
                fprintf(stderr, "unknown option/query predicate '%s' (arg #%d)\n",
                    argv[numArgsUsed], numArgsUsed);
                usage(1);
            }
            break;
          case kQEQueryErrorInvalidOrMissingArgument:
            fprintf(stderr, "invalid/missing option or argument for '%s' (arg #%d)\n",
                argv[numArgsUsed], numArgsUsed);
            break;
          case kQEQueryErrorParseCallbackFailed:
            fprintf(stderr, "query parsing callback failed\n");
            break;
          default:
            break;
        }
        goto finish;
    }

    if (!QEQueryIsComplete(query)) {
        fprintf(stderr, "unbalanced groups or trailing operator\n");
        goto finish;
    }

   /****************************************
    */
    if (argv[numArgsUsed] && !strcmp(argv[numArgsUsed], kKeywordReport)) {

        numArgsUsed++;

        if (argv[numArgsUsed] && !strcmp(argv[numArgsUsed], kNoReportHeader)) {
            numArgsUsed++;
            queryContext.reportStarted = true; // cause header to be skipped
        }

        if (queryContext.commandSpecified) {
            fprintf(stderr, "can't do report; query has commands\n");
            goto finish;
        }

        reportQuery = QEQueryCreate(&queryContext);
        if (!reportQuery) {
            fprintf(stderr, "Can't create report engine\n");
            goto finish;
        }
        QEQuerySetShortCircuits(reportQuery, false);

        while (reportCallback->longName) {
            if (reportCallback->parseCallback) {
                QEQuerySetParseCallbackForPredicate(reportQuery,
                    reportCallback->longName,
                    reportCallback->parseCallback);
                if (reportCallback->shortName) {
                    QEQuerySetSynonymForPredicate(reportQuery,
                        reportCallback->shortName,
                        reportCallback->longName);
                }
            }
            if (reportCallback->evalCallback) {
                QEQuerySetEvaluationCallbackForPredicate(reportQuery,
                    reportCallback->longName,
                    reportCallback->evalCallback);
            }
            reportCallback++;
        }

        reportStartIndex = numArgsUsed;

        while (QEQueryAppendElementFromArgs(reportQuery, argc - numArgsUsed,
            &argv[numArgsUsed], &numArgsUsed)) {
        }

        if (reportStartIndex == numArgsUsed) {
            fprintf(stderr, "no report predicates specified\n");
            usage(1);
            goto finish;
        }

        if (QEQueryLastError(reportQuery) != kQEQueryErrorNone) {
            switch (QEQueryLastError(reportQuery)) {
              case kQEQueryErrorNoMemory:
                fprintf(stderr, "memory allocation failure\n");
                break;
              case kQEQueryErrorEmptyGroup:
                fprintf(stderr, "empty group near arg #%d\n", numArgsUsed);
                break;
              case kQEQueryErrorSyntax:
                fprintf(stderr, "report syntax error near '%s' (arg #%d)\n",
                    argv[numArgsUsed], numArgsUsed);
                break;
              case kQEQueryErrorNoParseCallback:
                if (1) {
                    fprintf(stderr, "expected report predicate, found '%s' (arg #%d)\n",
                        argv[numArgsUsed], numArgsUsed);
                } else {
                    fprintf(stderr, "unknown option/report predicate '%s' (arg #%d)\n",
                        argv[numArgsUsed], numArgsUsed);
                }
                break;
              case kQEQueryErrorInvalidOrMissingArgument:
                fprintf(stderr, "invalid/missing option or argument for '%s' (arg #%d)\n",
                    argv[numArgsUsed], numArgsUsed);
                break;
              case kQEQueryErrorParseCallbackFailed:
                fprintf(stderr, "query parsing callback failed\n");
                break;
              default:
                break;
            }
            goto finish;
        }

        if (!QEQueryIsComplete(reportQuery)) {
            fprintf(stderr, "unbalanced groups or trailing operator\n");
            goto finish;
        }
    }

    if (numArgsUsed < argc) {
        fprintf(stderr, "leftover elements '%s'... (arg #%d)\n",
            argv[numArgsUsed], numArgsUsed);
        goto finish;
    }

   /*****
    * Set up the kext manager.
    */
    theKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!theKextManager) {
        qerror("can't allocate kernel extension manager\n");
        goto finish;
    }

    kmErr = KXKextManagerInit(theKextManager, true /* load_in_task */,
        false /* safe_boot_mode */);
    if (kmErr != kKXKextManagerErrorNone) {
        qerror("can't initialize kernel extension manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(kmErr));
        goto finish;
    }

    // XXX: set only based on find criteria???
    KXKextManagerSetPerformsFullTests(theKextManager, true);
    KXKextManagerSetPerformsStrictAuthentication(theKextManager, true);
    KXKextManagerSetLogLevel(theKextManager, kKXKextManagerLogLevelSilent);
    KXKextManagerSetLogFunction(theKextManager, &verbose_log);
    KXKextManagerSetErrorLogFunction(theKextManager, &error_log);
//    KXKextManagerSetUserVetoFunction(theKextManager, &user_approve);
//    KXKextManagerSetUserApproveFunction(theKextManager, &user_approve);
//    KXKextManagerSetUserInputFunction(theKextManager, &user_input);

   /*****
    * Disable clearing of relationships until we're done putting everything
    * together.
    */
    KXKextManagerDisableClearRelationships(theKextManager);

   /*****
    * Add the extensions folders to the manager.
    */
    count = CFArrayGetCount(repositoryDirectories);
    if (count == 0 && CFArrayGetCount(kextNames) == 0) {
        CFArrayAppendValue(repositoryDirectories, kKXSystemExtensionsFolder);
    }

    count = CFArrayGetCount(repositoryDirectories);
    for (i = 0; i < count; i++) {
        CFStringRef directory = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, i);
        CFURLRef directoryURL =
            CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                directory, kCFURLPOSIXPathStyle, true);
        if (!directoryURL) {
            qerror("memory allocation failure\n");
            goto finish;
        }

        kmErr = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            false /* use_repository_caches */, NULL);
        if (kmErr != kKXKextManagerErrorNone) {
            qerror("can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(kmErr));
            goto finish;
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

   /*****
    * Add each kext named on the command line to the manager. If any can't
    * be added, addKextsToManager() returns 0, so set the exit_code to 1, but
    * go on to process the kexts that could be added.
    * If a fatal error occurs, addKextsToManager() returns -1, so go right
    * to the finish;
    */
    addKextsResult = addKextsToManager(theKextManager, kextNames,
       NULL /* kextNamesToUse */, false /* do_tests */);
    if (addKextsResult < 1) {
        if (addKextsResult < 0) {
            goto finish;
        }
    }

    if (queryContext.checkAuthentic || queryContext.checkLoadable) {
        KXKextManagerAuthenticateKexts(theKextManager);
    }

    if (queryContext.checkIntegrity) {
        KXKextManagerVerifyIntegrityOfAllKexts(theKextManager);
    }

    KXKextManagerEnableClearRelationships(theKextManager);

    KXKextManagerCalculateVersionRelationships(theKextManager);
    KXKextManagerResolveAllKextDependencies(theKextManager);

    if (queryContext.checkLoaded) {
        KXKextManagerCheckForLoadedKexts(theKextManager);
    }

    /********* ********** ********** **********
    ********** ********** ********** *********/

    candidateKexts = KXKextManagerCopyAllKexts(theKextManager);
    if (!candidateKexts) {
        qerror("internal error\n");
        goto finish;
    }

   /*****
    * Run the query!
    */
    count = CFArrayGetCount(candidateKexts);
    for (i = 0; i < count; i++) {

        theKext = (KXKextRef)CFArrayGetValueAtIndex(candidateKexts, i);

        if (QEQueryEvaluate(query, theKext)) {
            if (!queryContext.commandSpecified) {
                if (!reportQuery) {
                    printKext(theKext, queryContext.pathSpec,
                        queryContext.extraInfo, '\n');
                } else {
                    if (!queryContext.reportStarted) {
                        queryContext.reportRowStarted = false;
                        QEQueryEvaluate(reportQuery, theKext);
                        printf("\n");
                        if ((QEQueryLastError(reportQuery) != kQEQueryErrorNone)) {
                            fprintf(stderr, "report evaluation error; aborting\n");
                            goto finish;
                        }
                        queryContext.reportStarted = true;
                    }
                    queryContext.reportRowStarted = false;
                    QEQueryEvaluate(reportQuery, theKext);
                    printf("\n");
                    if ((QEQueryLastError(reportQuery) != kQEQueryErrorNone)) {
                        fprintf(stderr, "report evaluation error; aborting\n");
                        goto finish;
                    }
                }
            }
        } else if (QEQueryLastError(query) != kQEQueryErrorNone) {
            fprintf(stderr, "query evaluation error; aborting\n");
            goto finish;
        }
    }

    exit_code = 0;

finish:

    if (query)                 QEQueryFree(query);
    if (repositoryDirectories) CFRelease(repositoryDirectories);
    if (kextNames)             CFRelease(kextNames);
    if (theKextManager)        CFRelease(theKextManager);
    if (candidateKexts)        CFRelease(candidateKexts);

    exit(exit_code);
    return exit_code;
}

/*******************************************************************************
*******************************************************************************/
Boolean addSearchItem(
    const char * optarg,
    CFMutableArrayRef repositoryDirectories,
    CFMutableArrayRef kextNames)
{
    Boolean result = false;
    CFStringRef name = NULL;   // must release

    name = CFStringCreateWithCString(kCFAllocatorDefault,
            optarg, kCFStringEncodingUTF8);
    if (!name) {
        qerror(kErrorStringMemoryAllocation);
        goto finish;
    }
    if (CFStringHasSuffix(name, CFSTR(kKextSuffix))) {
        CFArrayAppendValue(kextNames, name);
    } else {
        CFArrayAppendValue(repositoryDirectories, name);
    }
    result = true;
finish:
    if (name) CFRelease(name);
    return result;
}

/*******************************************************************************
* allocateArray()
*******************************************************************************/
static int allocateArray(CFMutableArrayRef * array) {

    int result = 1;  // assume success

    if (!array) {
        qerror("internal error\n");
        result = 0;
        goto finish;
    }

    *array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!*array) {
        result = 0;
        qerror(kErrorStringMemoryAllocation);
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* usage()
*******************************************************************************/
static void usage(int level)
{
    FILE * stream = stderr;

    fprintf(stream,
      "usage: %s [options] [directory or extension ...] [query]\n"
      "    [-report [-no-header] report_predicate...]"
      "\n",
      progname);

    if (level < 1) {
        return;
    }

    if (level == 1) {
        fprintf(stream, "use %s -%s for a list of options\n",
            progname, kOptNameHelp);
        return;
    }

    fprintf(stream, "Options\n");

    fprintf(stream, "    -%s                        -%s\n",
        kOptNameHelp, kOptNameCaseInsensitive);
#ifdef EXTRA_INFO
    fprintf(stream, "    -%s                  -%s\n",
        kOptNameExtraInfo, kOptNameNulTerminate);
#endif
    fprintf(stream, "    -%s              -%s\n",
        kOptNameRelativePaths, kOptNameSubstring);
    fprintf(stream, "    -%s\n",
        kOptNameNoPaths);

    fprintf(stream, "\n");

    fprintf(stream, "Handy Query Predicates\n");

    fprintf(stream, "    %s [-s] [-i] id\n", kPredNameBundleID);
    fprintf(stream, "    %s [-s] [-i] id\n", kPredNameBundleName);
    fprintf(stream, "    %s [-s] [-i] name value\n", kPredNameMatchProperty);
    fprintf(stream, "    %s [-s] [-i] name value\n", kPredNameProperty);

    fprintf(stream, "\n");

    fprintf(stream, "    %s                      %s\n",
        kPredNameLoaded, kPredNameNonloadable);
    fprintf(stream, "    %s                     %s\n",
        kPredNameInvalid, kPredNameInauthentic);
    fprintf(stream, "    %s        %s\n",
        kPredNameDependenciesMissing, kPredNameWarnings);

    fprintf(stream, "\n");

    fprintf(stream, "    %s arch1[,arch2...]       %s arch1[,arch2...]\n",
        kPredNameArch, kPredNameArchExact);
    fprintf(stream, "    %s                  %s\n",
        kPredNameExecutable, kPredNameIsLibrary);
    fprintf(stream, "    %s symbol       %s symbol\n",
        kPredNameDefinesSymbol, kPredNameReferencesSymbol);

    fprintf(stream, "\n");

    fprintf(stream, "See the man page for the full list.\n");

    return;
}
