/*
 * Copyright (c) 2002-2024 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __mDNSDebug_h
#define __mDNSDebug_h

#include "mDNSFeatures.h"


// Set MDNS_DEBUGMSGS to 0 to optimize debugf() calls out of the compiled code
// Set MDNS_DEBUGMSGS to 1 to generate normal debugging messages
// Set MDNS_DEBUGMSGS to 2 to generate verbose debugging messages
// MDNS_DEBUGMSGS is normally set in the project options (or makefile) but can also be set here if desired
// (If you edit the file here to turn on MDNS_DEBUGMSGS while you're debugging some code, be careful
// not to accidentally check-in that change by mistake when you check in your other changes.)

#ifndef MDNS_DEBUGMSGS
#define MDNS_DEBUGMSGS 0
#endif

// Set MDNS_CHECK_PRINTF_STYLE_FUNCTIONS to 1 to enable extra GCC compiler warnings
// Note: You don't normally want to do this, because it generates a bunch of
// spurious warnings for the following custom extensions implemented by mDNS_vsnprintf:
//    warning: `#' flag used with `%s' printf format    (for %#s              -- pascal string format)
//    warning: repeated `#' flag in format              (for %##s             -- DNS name string format)
//    warning: double format, pointer arg (arg 2)       (for %.4a, %.16a, %#a -- IP address formats)
#define MDNS_CHECK_PRINTF_STYLE_FUNCTIONS 0

typedef const char * mDNSLogCategory_t;
typedef enum
{
    MDNS_LOG_FAULT   = 1,
    MDNS_LOG_ERROR   = 2,
    MDNS_LOG_WARNING = 3,
    MDNS_LOG_DEFAULT = 4,
    MDNS_LOG_INFO    = 5,
    MDNS_LOG_DEBUG   = 6
} mDNSLogLevel_t;

    #define MDNS_LOG_CATEGORY_DEFINITION(NAME)  # NAME

#define MDNS_LOG_CATEGORY_DEFAULT   MDNS_LOG_CATEGORY_DEFINITION(Default)
#define MDNS_LOG_CATEGORY_STATE     MDNS_LOG_CATEGORY_DEFINITION(State)
#define MDNS_LOG_CATEGORY_MDNS      MDNS_LOG_CATEGORY_DEFINITION(mDNS)
#define MDNS_LOG_CATEGORY_UDNS      MDNS_LOG_CATEGORY_DEFINITION(uDNS)
#define MDNS_LOG_CATEGORY_SPS       MDNS_LOG_CATEGORY_DEFINITION(SPS)
#define MDNS_LOG_CATEGORY_NAT       MDNS_LOG_CATEGORY_DEFINITION(NAT)
#define MDNS_LOG_CATEGORY_D2D       MDNS_LOG_CATEGORY_DEFINITION(D2D)
#define MDNS_LOG_CATEGORY_XPC       MDNS_LOG_CATEGORY_DEFINITION(XPC)
#define MDNS_LOG_CATEGORY_ANALYTICS MDNS_LOG_CATEGORY_DEFINITION(Analytics)
#define MDNS_LOG_CATEGORY_DNSSEC    MDNS_LOG_CATEGORY_DEFINITION(DNSSEC)

// Use MDNS_LOG_CATEGORY_DISABLED to disable a log temporarily.
    extern const char mDNS_LogDisabled[];
    #define MDNS_LOG_CATEGORY_DISABLED ((const char *)mDNS_LogDisabled)

// Set this symbol to 1 to answer remote queries for our Address, and reverse mapping PTR
#define ANSWER_REMOTE_HOSTNAME_QUERIES 0

// Set this symbol to 1 to do extra debug checks on malloc() and free()
// Set this symbol to 2 to write a log message for every malloc() and free()
#ifndef MDNS_MALLOC_DEBUGGING
#define MDNS_MALLOC_DEBUGGING 0
#endif

#if (MDNS_MALLOC_DEBUGGING > 0) && defined(WIN32)
#error "Malloc debugging does not yet work on Windows"
#endif

#define ForceAlerts 0
//#define LogTimeStamps 1

// Developer-settings section ends here

#if MDNS_CHECK_PRINTF_STYLE_FUNCTIONS
#define IS_A_PRINTF_STYLE_FUNCTION(F,A) __attribute__ ((format(printf,F,A)))
#else
#define IS_A_PRINTF_STYLE_FUNCTION(F,A)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Variable argument macro support. Use ANSI C99 __VA_ARGS__ where possible. Otherwise, use the next best thing.

#if (defined(__GNUC__))
    #if ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2)))
        #define MDNS_C99_VA_ARGS        1
        #define MDNS_GNU_VA_ARGS        0
    #else
        #define MDNS_C99_VA_ARGS        0
        #define MDNS_GNU_VA_ARGS        1
    #endif
    #define MDNS_HAS_VA_ARG_MACROS      1
#elif (_MSC_VER >= 1400) // Visual Studio 2005 and later
    #define MDNS_C99_VA_ARGS            1
    #define MDNS_GNU_VA_ARGS            0
    #define MDNS_HAS_VA_ARG_MACROS      1
#elif (defined(__MWERKS__))
    #define MDNS_C99_VA_ARGS            1
    #define MDNS_GNU_VA_ARGS            0
    #define MDNS_HAS_VA_ARG_MACROS      1
#else
    #define MDNS_C99_VA_ARGS            0
    #define MDNS_GNU_VA_ARGS            0
    #define MDNS_HAS_VA_ARG_MACROS      0
#endif

#if (MDNS_HAS_VA_ARG_MACROS)
    #if (MDNS_C99_VA_ARGS)
        #define MDNS_LOG_DEFINITION(LEVEL, ...) \
            do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_CATEGORY_DEFAULT, LEVEL, __VA_ARGS__); } while (0)

        #define debug_noop(...)   do {} while(0)
        #define LogMsg(...)       LogMsgWithLevel(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT, __VA_ARGS__)
        #define LogOperation(...) MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  __VA_ARGS__)
        #define LogSPS(...)       MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  __VA_ARGS__)
        #define LogInfo(...)      MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  __VA_ARGS__)
        #define LogDebug(...)     MDNS_LOG_DEFINITION(MDNS_LOG_DEBUG, __VA_ARGS__)
    #elif (MDNS_GNU_VA_ARGS)
        #define MDNS_LOG_DEFINITION(LEVEL, ARGS...) \
            do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_CATEGORY_DEFAULT, LEVEL, ARGS); } while (0)

        #define debug_noop(ARGS...)   do {} while (0)
        #define LogMsg(ARGS... )      LogMsgWithLevel(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT, ARGS)
        #define LogOperation(ARGS...) MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  ARGS)
        #define LogSPS(ARGS...)       MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  ARGS)
        #define LogInfo(ARGS...)      MDNS_LOG_DEFINITION(MDNS_LOG_DEFAULT,  ARGS)
        #define LogDebug(ARGS...)     MDNS_LOG_DEFINITION(MDNS_LOG_DEBUG, ARGS)
    #else
        #error "Unknown variadic macros"
    #endif
#else
// If your platform does not support variadic macros, you need to define the following variadic functions.
// See mDNSShared/mDNSDebug.c for sample implementation
    #define debug_noop 1 ? (void)0 : (void)
    #define LogMsg LogMsg_
    #define LogOperation (mDNS_LoggingEnabled == 0) ? ((void)0) : LogOperation_
    #define LogSPS       (mDNS_LoggingEnabled == 0) ? ((void)0) : LogSPS_
    #define LogInfo      (mDNS_LoggingEnabled == 0) ? ((void)0) : LogInfo_
    #define LogDebug     (mDNS_LoggingEnabled == 0) ? ((void)0) : LogDebug_
extern void LogMsg_(const char *format, ...)       IS_A_PRINTF_STYLE_FUNCTION(1,2);
extern void LogOperation_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
extern void LogSPS_(const char *format, ...)       IS_A_PRINTF_STYLE_FUNCTION(1,2);
extern void LogInfo_(const char *format, ...)      IS_A_PRINTF_STYLE_FUNCTION(1,2);
extern void LogDebug_(const char *format, ...)     IS_A_PRINTF_STYLE_FUNCTION(1,2);
#endif


#if MDNS_DEBUGMSGS
#define debugf debugf_
extern void debugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else
#define debugf debug_noop
#endif

#if MDNS_DEBUGMSGS > 1
#define verbosedebugf verbosedebugf_
extern void verbosedebugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else
#define verbosedebugf debug_noop
#endif

extern int mDNS_LoggingEnabled;
extern int mDNS_DebugLoggingEnabled;
extern int mDNS_PacketLoggingEnabled;
extern int mDNS_McastLoggingEnabled;
extern int mDNS_McastTracingEnabled;
extern int mDNS_DebugMode;          // If non-zero, LogMsg() writes to stderr instead of syslog


extern const char ProgramName[];

extern void LogMsgWithLevel(mDNSLogCategory_t category, mDNSLogLevel_t level, const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(3,4);
// LogMsgNoIdent needs to be fixed so that it logs without the ident prefix like it used to
// (or completely overhauled to use the new "log to a separate file" facility)
#define LogMsgNoIdent LogMsg

#define LogFatalError LogMsg

#if MDNS_MALLOC_DEBUGGING >= 1
extern void *mallocL(const char *msg, mDNSu32 size);
extern void *callocL(const char *msg, mDNSu32 size);
extern void freeL(const char *msg, void *x);
#define LogMemCorruption LogMsg
#else
#define mallocL(MSG, SIZE) mdns_malloc(SIZE)
#define callocL(MSG, SIZE) mdns_calloc(1, SIZE)
#define freeL(MSG, PTR) mdns_free(PTR)
#endif // MDNS_MALLOC_DEBUGGING >= 1

#ifdef __cplusplus
}
#endif

    #if (MDNS_HAS_VA_ARG_MACROS)
        #if (MDNS_C99_VA_ARGS)
            #define LogRedact(CATEGORY, LEVEL, ...) \
                do { if (mDNS_LoggingEnabled) LogMsgWithLevel(CATEGORY, LEVEL, __VA_ARGS__); } while (0)
        #elif (MDNS_GNU_VA_ARGS)
            #define LogRedact(CATEGORY, LEVEL, ARGS...) \
                do { if (mDNS_LoggingEnabled) LogMsgWithLevel(CATEGORY, LEVEL, ARGS); } while (0)
        #else
            #error "Unknown variadic macros"
        #endif
    #else
        #define LogRedact      (mDNS_LoggingEnabled == 0) ? ((void)0) : LogRedact_
        extern void LogRedact_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
    #endif

//======================================================================================================================
// MARK: - RData Log Helper

    #define MDNS_CORE_LOG_RDATA_WITH_BUFFER(CATEGORY, LEVEL, RR_PTR, RDATA_BUF, RDATA_BUF_LEN, FORMAT, ...)         \
        do                                                                                                          \
        {                                                                                                           \
            (void)(RDATA_BUF);                                                                                      \
            (void)(RDATA_BUF_LEN);                                                                                  \
            LogRedact(CATEGORY, LEVEL, FORMAT " " PRI_S, ##__VA_ARGS__, RRDisplayString(&mDNSStorage, (RR_PTR)));   \
        } while (0)

#define MDNS_CORE_LOG_RDATA(CATEGORY, LEVEL, RR_PTR, FORMAT, ...)                                                   \
    do                                                                                                              \
    {                                                                                                               \
        mDNSu8 *_rdataBuffer = NULL;                                                                                \
        mDNSu8 *_rdataBufferHeap = NULL;                                                                            \
        mDNSu16 _rdataBufferLen;                                                                                    \
        if ((RR_PTR)->rdlength <= sizeof(mDNSStorage.RDataBuffer))                                                  \
        {                                                                                                           \
            _rdataBuffer = mDNSStorage.RDataBuffer;                                                                 \
            _rdataBufferLen = sizeof(mDNSStorage.RDataBuffer);                                                      \
        }                                                                                                           \
        else                                                                                                        \
        {                                                                                                           \
            _rdataBufferHeap = mDNSPlatformMemAllocate((RR_PTR)->rdlength);                                         \
            _rdataBuffer = _rdataBufferHeap;                                                                        \
            _rdataBufferLen = (RR_PTR)->rdlength;                                                                   \
        }                                                                                                           \
        if ((RR_PTR)->rdlength == 0)                                                                                \
        {                                                                                                           \
            LogRedact(CATEGORY, LEVEL,                                                                              \
                FORMAT "type: " PUB_DNS_TYPE ", rdata: <none>", ##__VA_ARGS__, DNS_TYPE_PARAM((RR_PTR)->rrtype));   \
        }                                                                                                           \
        else                                                                                                        \
        {                                                                                                           \
            MDNS_CORE_LOG_RDATA_WITH_BUFFER(CATEGORY, LEVEL, RR_PTR, _rdataBuffer, _rdataBufferLen, FORMAT,         \
                ##__VA_ARGS__);                                                                                     \
        }                                                                                                           \
        mDNSPlatformMemFree(_rdataBufferHeap);                                                                      \
    }                                                                                                               \
    while(0)

//======================================================================================================================
// MARK: - Customized Log Specifier

// The followings are the customized log specifier defined in os_log. For compatibility, we have to define it when it is
// not on the Apple platform, for example, the Posix platform. The keyword "public" or "private" is used to control whether
// the content would be redacted when the redaction is turned on: "public" means the content will always be printed;
// "private" means the content will be printed as <mask.hash: '<The hashed string from binary data>'> if the redaction is turned on,
// only when the redaction is turned off, the content will be printed as what it should be. Note that the hash performed
// to the data is a salted hashing transformation, and the salt is generated randomly on a per-process basis, meaning
// that hashes cannot be correlated across processes or devices.

    #define PRI_PREFIX

    #define PUB_S "%s"
    #define PRI_S PUB_S

    #define PUB_BOOL                    PUB_S
    #define BOOL_PARAM(boolean_value)   ((boolean_value) ? "yes" : "no")

    #define PUB_TIMEV                   "%ld"
    #define TIMEV_PARAM(time_val_ptr)   ((time_val_ptr)->tv_sec)

    #define DNS_MSG_ID_FLAGS                                "id: 0x%04X (%u), flags: 0x%04X"
    #define DNS_MSG_ID_FLAGS_PARAM(HEADER, ID_AND_FLAGS)    mDNSVal16((HEADER).id), mDNSVal16((HEADER).id), \
                                                                ((HEADER).flags.b)

    #define DNS_MSG_COUNTS                          "counts: %u/%u/%u/%u"
    #define DNS_MSG_COUNTS_PARAM(HEADER, COUNTS)    (HEADER).numQuestions, (HEADER).numAnswers, \
                                                        (HEADER).numAuthorities, (HEADER).numAdditionals

    // If os_log is not supported, there is no way to parse the name hash type bytes.
    #define MDNS_NAME_HASH_TYPE_BYTES                           "%s"
    #define MDNS_NAME_HASH_TYPE_BYTES_PARAM(BYTES, BYTES_LEN)   ""

    #define PUB_DNS_TYPE                PUB_S
    #define DNS_TYPE_PARAM(type_value)  (DNSTypeName(type_value))

// Notes about using RMV rather than REMOVE:
// Both "add" and "rmv" are three characters so that when the log is printed, the content will be aligned which is good
// for log searching. For example:
// DNSServiceBrowse(_test._tcp.local., PTR) RESULT ADD interface 1:   23 _test._tcp.local. PTR demo._test._tcp.local.
// DNSServiceBrowse(_test._tcp.local., PTR) RESULT RMV interface 1:   23 _test._tcp.local. PTR demo._test._tcp.local.
// is better than:
// DNSServiceBrowse(_test._tcp.local., PTR) RESULT ADD interface 1:   23 _test._tcp.local. PTR demo._test._tcp.local.
// DNSServiceBrowse(_test._tcp.local., PTR) RESULT REMOVE interface 1:   23 _test._tcp.local. PTR demo._test._tcp.local.
    #define PUB_ADD_RMV                     PUB_S
    #define ADD_RMV_PARAM(add_rmv_value)    ((add_rmv_value) ? "add" : "rmv")

// Here we have the uppercase style so that it can be used to match the original mDNSResponder RESULT ADD/RMV all
// uppercase.
    #define PUB_ADD_RMV_U                   PUB_S
    #define ADD_RMV_U_PARAM(add_rmv_value)  ((add_rmv_value) ? "ADD" : "RMV")

    #define PUB_PN                      PUB_S
    #define PN_PARAM(pn_boolean_value)  ((pn_boolean_value) ? "positive" : "negative")

    #define PUB_MORTALITY                       PUB_S
    #define MORTALITY_PARAM(mortality_value)    (MortalityDisplayString(mortality_value))

    #define PUB_DM_NAME                 "%##s"
    #define PRI_DM_NAME                 PUB_DM_NAME
    #define DM_NAME_PARAM(name)         (name)
    #define DM_NAME_PARAM_NONNULL(name) (name)

    #define PUB_DM_LABEL                "%#s"
    #define PRI_DM_LABEL                PUB_DM_LABEL
    #define DM_LABEL_PARAM(label)       (label)
    #define DM_LABEL_PARAM_SAFE(label)  (label)

    #define PUB_IP_ADDR "%#a"
    #define PRI_IP_ADDR PUB_IP_ADDR

    #define PUB_IPv4_ADDR "%.4a"
    #define PRI_IPv4_ADDR PUB_IPv4_ADDR

    #define PUB_IPv6_ADDR "%.16a"
    #define PRI_IPv6_ADDR PUB_IPv6_ADDR

    #define PUB_MAC_ADDR "%.6a"
    #define PRI_MAC_ADDR PUB_MAC_ADDR

    #define PUB_HEX "%p"
    #define PRI_HEX PUB_HEX
    #define HEX_PARAM(hex, hex_length) (hex)

    #define PUB_DNSKEY "%p"
    #define PRI_DNSKEY PUB_DNSKEY
    #define DNSKEY_PARAM(rdata, rdata_length) (rdata)

    #define PUB_DS "%p"
    #define PRI_DS PUB_DS
    #define DS_PARAM(rdata, rdata_length) (rdata)

    #define PUB_NSEC "%p"
    #define PRI_NSEC PUB_NSEC
    #define NSEC_PARAM(rdata, rdata_length) (rdata)

    #define PUB_NSEC3 "%p"
    #define PRI_NSEC3 PUB_NSEC3
    #define NSEC3_PARAM(rdata, rdata_length) (rdata)

    #define PUB_RRSIG "%p"
    #define PRI_RRSIG PUB_RRSIG
    #define RRSIG_PARAM(rdata, rdata_length) (rdata)

    #define PUB_SVCB "%p"
    #define PRI_SVCB PUB_SVCB
    #define SVCB_PARAM(rdata, rdata_length) (rdata)

        #define PUB_DNSSEC_RESULT                           "%s"
        #define DNSSEC_RESULT_PARAM(dnssec_result_value)    ("<DNSSEC Unsupported>")

        #define PUB_DNSSEC_INVAL_STATE                  "%s"
        #define DNSSEC_INVAL_STATE_PARAM(state_value)   ("<DNSSEC Unsupported>")

    #define PUB_TIME_DUR    "%us"

    #define PUB_OS_ERR    "%ld"

    #define PUB_RDATA       "%p"
    #define PRI_RDATA       PUB_RDATA
    #define RDATA_PARAM(buf, buf_len, rrtype, rdata, rdata_len) (rdata)

    #define PUB_TYPE_RDATA  PUB_S " %p"
    #define PRI_TYPE_RDATA  PUB_TYPE_RDATA
    #define TYPE_RDATA_PARAM(buf, buf_len, rrtype, rdata, rdata_len) \
                            DNSTypeName(rrtype), RDATA_PARAM(buf, buf_len, rrtype, rdata, rdata_len)


    #define PUB_DNS_SCOPE_TYPE          "%s"
    #define DNS_SCOPE_TYPE_PARAM(type)  DNSScopeToString(type)

extern void LogToFD(int fd, const char *format, ...);

#endif // __mDNSDebug_h
