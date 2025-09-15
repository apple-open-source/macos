/* srp.h
 *
 * Copyright (c) 2018-2025 Apple Inc. All rights reserved.
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
 *
 * Service Registration Protocol common definitions
 */

#ifndef __SRP_H
#define __SRP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef THREAD_DEVKIT_ADK
#include <netinet/in.h>
#endif
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifdef POSIX_BUILD
#include <limits.h>
#include <sys/param.h>
#endif
#ifdef MALLOC_DEBUG_LOGGING
#  define MDNS_NO_STRICT 1
#endif

#include "srp-features.h"           // for feature flags


/*!
 *  @brief
 *      Evaluates to non-zero if the compiler is Clang.
 *
 *  @discussion
 *      __clang__ is defined when compiling with Clang, see
 *      <https://clang.llvm.org/docs/LanguageExtensions.html#builtin-macros>.
 */
#if defined(__clang__)
#   define SRP_COMPILER_IS_CLANG() 1
#else
#   define SRP_COMPILER_IS_CLANG() 0
#endif

#ifdef __clang__
#define NULLABLE _Nullable
#define NONNULL _Nonnull
#define UNUSED __unused
#else
#define NULLABLE
#define NONNULL
#define UNUSED __attribute__((unused))
#ifdef POSIX_BUILD
#else
#define SRP_CRYPTO_MBEDTLS 1
#endif // POSIX_BUILD
#endif

#define INT64_HEX_STRING_MAX 17     // Maximum size of an int64_t printed as hex, including NUL termination

/*!
 *  @brief
 *      Stringizes the argument and passes it to the _Pragma() operator, which takes a string literal argument.
 *
 *  @param ARG
 *      The argument.
 *
 *  @discussion
 *      Useful for escaping double quotes. For example,
 *
 *          SRP_PRAGMA_WITH_STRINGIZED_ARGUMENT(clang diagnostic ignored "-Wpadded")
 *
 *      turns into
 *
 *          _Pragma("clang diagnostic ignored \"-Wpadded\"")
 *
 *      See <https://gcc.gnu.org/onlinedocs/cpp/Pragmas.html>.
 */
#define SRP_PRAGMA_WITH_STRINGIZED_ARGUMENT(ARG)    _Pragma(#ARG)

/*!
 *    @brief
 *        For Clang, starts ignoring the specified warning diagnostic flag.
 *
 *    @param WARNING
 *        The warning diagnostic flag.
 *
 *    @discussion
 *        Use SRP_CLANG_TREAT_WARNING_AS_ERROR_END() to undo the effect of this macro.
 */
#if SRP_COMPILER_IS_CLANG()
#   define SRP_CLANG_TREAT_WARNING_AS_ERROR_BEGIN(WARNING) \
        _Pragma("clang diagnostic push")                    \
        SRP_PRAGMA_WITH_STRINGIZED_ARGUMENT(clang diagnostic error #WARNING)
#else
#   define SRP_CLANG_TREAT_WARNING_AS_ERROR_BEGIN(WARNING)
#endif

/*!
 *    @brief
 *        Use to undo the effect of a previous SRP_CLANG_TREAT_WARNING_AS_ERROR_BEGIN().
 */
#if SRP_COMPILER_IS_CLANG()
#   define SRP_CLANG_TREAT_WARNING_AS_ERROR_END()    _Pragma("clang diagnostic pop")
#else
#   define SRP_CLANG_TREAT_WARNING_AS_ERROR_END()
#endif

/*!
 *  @brief
 *      For Clang, starts ignoring the specified warning diagnostic flag.
 *
 *  @param WARNING
 *      The warning diagnostic flag.
 *
 *  @discussion
 *      Use SRP_CLANG_IGNORE_WARNING_END() to undo the effect of this macro.
 */
#if SRP_COMPILER_IS_CLANG()
#   define SRP_CLANG_IGNORE_WARNING_BEGIN(WARNING)                              \
        _Pragma("clang diagnostic push")                                        \
        SRP_PRAGMA_WITH_STRINGIZED_ARGUMENT(clang diagnostic ignored #WARNING)
#else
	#define SRP_CLANG_IGNORE_WARNING_BEGIN(WARNING)
#endif

/*!
 *	@brief
 *		Use to undo the effect of a previous MDNS_CLANG_IGNORE_WARNING_BEGIN().
 */
#if SRP_COMPILER_IS_CLANG()
	#define SRP_CLANG_IGNORE_WARNING_END()	_Pragma("clang diagnostic pop")
#else
	#define SRP_CLANG_IGNORE_WARNING_END()
#endif

/*!
 *  @brief
 *      Declares an array of bytes that is meant to be used as explicit padding at the end of a struct.
 *
 *  @param BYTE_COUNT
 *      The size of the array in number of bytes.
 *
 *  @discussion
 *      This explicit padding is meant to be used as the final member variable of a struct to eliminate the
 *      -Wpadded warning about a struct's overall size being implicitly padded up to an alignment boundary.
 *      In other words, in place of such implicit padding, use this explicit padding instead.
 */
#define SRP_STRUCT_PAD(BYTE_COUNT)                      \
    SRP_CLANG_IGNORE_WARNING_BEGIN(-Wzero-length-array) \
    char _srp_unused_padding[(BYTE_COUNT)]              \
    SRP_CLANG_IGNORE_WARNING_END()

/*!
 *  @brief
 *      Like SRP_STRUCT_PAD(), except that the amount of padding is specified for 64-bit and 32-bit platforms.
 *
 *  @param BYTE_COUNT_64
 *      The amount of padding in number of bytes to use on 64-bit platforms.
 *
 *  @param BYTE_COUNT_32
 *      The amount of padding in number of bytes to use on 32-bit platforms.
 *
 *  @discussion
 *      This macro assumes that pointers on 64-bit platforms are eight bytes in size and that pointers on 32-bit
 *      platforms are four bytes in size.
 *
 *      If the size of a pointer is something other than eight or four bytes, then a compiler error will occur.
 */
#define SRP_STRUCT_PAD_64_32(BYTE_COUNT_64, BYTE_COUNT_32)  \
    SRP_STRUCT_PAD(                                         \
        (sizeof(void *) == 8) ? BYTE_COUNT_64 :             \
        (sizeof(void *) == 4) ? BYTE_COUNT_32 : -1          \
    )

#include "srp-log.h"                // For log functions

#ifdef SRP_TEST_SERVER
void srp_log_ref_reset(void);
#endif
bool srp_log_ref_check(void *x, const char *object_type, const char *file, int line, bool fault);
void srp_log_ref_final(void *x, unsigned ref_count, const char *object_type, const char *file, int line);

#define SRP_OBJ_REF_COUNT_LIMIT    10000

#ifdef __clang__
#define FILE_TRIM(x) (strrchr(x, '/') + 1)
#else
#define FILE_TRIM(x) (x)
#endif

#ifdef THREAD_DEVKIT_ADK
#define FINALIZED(x)
#define CREATED(x)
#else
#define FINALIZED(x) ((x)++)
#define CREATED(x) ((x)++)
#endif // THREAD_DEVKIT_ADK


#ifdef __clang_analyzer__
#define OBJECT_TYPE(x) typedef struct x x##_t; void srp_##x##_release(x##_t *obj); void srp_##x##_retain(x##_t *obj);
#define NW_OBJECT_TYPE(x)
#include "object-types.h"
#undef OBJECT_TYPE
#undef NW_OBJECT_TYPE
#define RELEASE_BASE(x, object_type, check, final, file, line) srp_##object_type##_release(x)
#define RETAIN_BASE(x, object_type, check, final, file, line)  srp_##object_type##_retain(x)
#else
#define RELEASE_BASE(x, object_type, check, final, file, line) do {               \
        if ((x) != NULL && check(x, #object_type, file, line, true)) {            \
            UNUSED int ref_count = (x)->ref_count - 1;                            \
            if ((x)->ref_count == 0) {                                            \
                FAULT("ALLOC: release after finalize at %2.2d: %p (%10s): %s:%d", \
                      (x)->ref_count, (void *)(x), # x, FILE_TRIM(file), line);   \
                abort();                                                          \
            } else if ((x)->ref_count > SRP_OBJ_REF_COUNT_LIMIT) {                \
                FAULT("ALLOC: release at %2.2d: %p (%10s): %s:%d",                \
                      (x)->ref_count, (void *)(x), # x, FILE_TRIM(file), line);   \
                abort();                                                          \
            } else {                                                              \
                INFO("ALLOC: release at %2.2d: %p (%10s): %s:%d",                 \
                     (x)->ref_count, (void *)(x), # x, FILE_TRIM(file), line);    \
                --(x)->ref_count;                                                 \
                if ((x)->ref_count == 0) {                                        \
                    INFO("ALLOC:      finalize: %p (%10s): %s:%d",                \
                         (void *)(x), # x, FILE_TRIM(file), line);                \
                    FINALIZED(object_type##_finalized);                           \
                    object_type##_finalize(x);                                    \
                }                                                                 \
            }                                                                     \
            final(x, ref_count, #object_type, file, line);                        \
        }                                                                         \
    } while (0)

#define RETAIN_BASE(x, object_type, final, file, line) do {                       \
        if ((x) != NULL) {                                                        \
            INFO("ALLOC:  retain at %2.2d: %p (%10s): %s:%d",                     \
                 (x)->ref_count, (void *)(x), # x, FILE_TRIM(file), line);        \
            if ((x)->ref_count == 0) {                                            \
               CREATED(object_type##_created);                                    \
            }                                                                     \
            ++((x)->ref_count);                                                   \
            if ((x)->ref_count > SRP_OBJ_REF_COUNT_LIMIT) {                       \
                FAULT("ALLOC: retain at %2.2d: %p (%10s): %s:%d",                 \
                      (x)->ref_count, (void *)(x), # x, FILE_TRIM(file), line);   \
                abort();                                                          \
            }                                                                     \
            final(x, (x)->ref_count, #object_type, file, line);                    \
        }                                                                         \
    } while (0)
#endif // __clang_analyzer__
#define RELEASE_NOTHING(x, object_type, file, line, fault) true
#define RELEASE(x, object_type) RELEASE_BASE(x, object_type, RELEASE_NOTHING, RELEASE_NOTHING, file, line)
#define RETAIN(x, object_type) RETAIN_BASE(x, object_type, RELEASE_NOTHING, file, line)
#define RELEASE_HERE(x, object_type) RELEASE_BASE(x, object_type, RELEASE_NOTHING, RELEASE_NOTHING, __FILE__, __LINE__)
#define RETAIN_HERE(x, object_type) RETAIN_BASE(x, object_type, RELEASE_NOTHING, __FILE__, __LINE__)
#define RELEASE_LOG(x, object_type) RELEASE_BASE(x, object_type, srp_log_ref_check, srp_log_ref_final, file, line)
#define RETAIN_LOG(x, object_type) RETAIN_BASE(x, object_type, srp_log_ref_final, file, line)
#define RELEASE_HERE_LOG(x, object_type) RELEASE_BASE(x, object_type, srp_log_ref_check, srp_log_ref_final, __FILE__, __LINE__)
#define RETAIN_HERE_LOG(x, object_type) RETAIN_BASE(x, object_type, srp_log_ref_final, __FILE__, __LINE__)

#define THREAD_ENTERPRISE_NUMBER ((uint64_t)44970)
#define THREAD_SRP_SERVER_ANYCAST_OPTION 0x5c
#define THREAD_SRP_SERVER_OPTION 0x5d
#define THREAD_PREF_ID_OPTION    0x9d
#define APPLE_ENTERPRISE_NUMBER ((uint64_t)63)

#define IS_SRP_SERVICE(service) \
    ((cti_service)->enterprise_number == THREAD_ENTERPRISE_NUMBER &&    \
     (cti_service)->service_type == THREAD_SRP_SERVER_OPTION &&         \
     (cti_service)->service_version == 1 &&                             \
     (cti_service)->server_length == 18)
#define IS_SRP_ANYCAST_SERVICE(service) \
    ((cti_service)->enterprise_number == THREAD_ENTERPRISE_NUMBER &&    \
     (cti_service)->service_type == THREAD_SRP_SERVER_ANYCAST_OPTION && \
     (cti_service)->service_version == 1 &&                             \
     (cti_service)->service_length == 2)
#define IS_PREF_ID_SERVICE(service) \
    ((cti_service)->enterprise_number == THREAD_ENTERPRISE_NUMBER &&    \
     (cti_service)->service_type == THREAD_PREF_ID_OPTION &&            \
     (cti_service)->service_version == 1 &&                             \
     (cti_service)->server_length == 9)
#define IS_APPLE_FLAG_SERVICE(service) \
    ((cti_service)->enterprise_number == APPLE_ENTERPRISE_NUMBER &&    \
     (cti_service)->service_type == THREAD_APPLE_BR_SIGNAL &&          \
     (cti_service)->service_version == 1 &&                            \
     (cti_service)->server_length == 0)
#ifdef MALLOC_DEBUG_LOGGING
void *debug_malloc(size_t len, const char *file, int line);
void *debug_calloc(size_t count, size_t len, const char *file, int line);
char *debug_strdup(const char *s, const char *file, int line);
void debug_free(void *p, const char *file, int line);

#define malloc(x) debug_malloc(x, __FILE__, __LINE__)
#define calloc(c, y) debug_calloc(c, y, __FILE__, __LINE__)
#define strdup(s) debug_strdup(s, __FILE__, __LINE__)
#define free(p) debug_free(p, __FILE__, __LINE__)
#endif

typedef struct srp_key srp_key_t;

// This function compares two IPv6 prefixes, up to the specified prefix length (in bytes).
// return: -1 if prefix_a < prefix_b
//          0 if prefix_a == prefix_b
//          1 if prefix_a > prefix_b.
static inline int
in6prefix_compare(const struct in6_addr *prefix_a, const struct in6_addr *prefix_b, size_t len)
{
    return memcmp(prefix_a, prefix_b, len);
}

// This function compares two full IPv6 addresses.
// return: -1 if addr_a < addr_b
//          0 if addr_a == addr_b
//          1 if addr_a > addr_b.
static inline int
in6addr_compare(const struct in6_addr *addr_a, const struct in6_addr *addr_b)
{
    return in6prefix_compare(addr_a, addr_b, sizeof (*addr_a));
}

// This function copies the data into a, up to len bytes or sizeof(*a), whichever is less.
// if there are uninitialized bytes remaining in a, sets those to zero.
static inline void
in6prefix_copy_from_data(struct in6_addr *prefix, const uint8_t *data, size_t len)
{
    size_t copy_len = sizeof(*prefix) < len ? sizeof(*prefix): len;
    if (copy_len > 0) {
        memcpy(prefix, data, copy_len);
    }
    if (copy_len != sizeof(*prefix)) {
        memset((char *)prefix + copy_len, 0, sizeof(*prefix) - copy_len);
    }
}

// This function copies prefix src, into prefix dst, up to len bytes.
static inline void
in6prefix_copy(struct in6_addr *dst, const struct in6_addr *src, size_t len)
{
    in6prefix_copy_from_data(dst, (const uint8_t*)src, len);
}

// This function copies full IPv6 address src into dst.
static inline void
in6addr_copy(struct in6_addr *dst, const struct in6_addr *src)
{
    memcpy(dst, src, sizeof(*dst));
}

// This function zeros the full IPv6 address
static inline void
in6addr_zero(struct in6_addr *addr)
{
    memset(addr, 0, sizeof(*addr));
}


/*!
 *  @brief
 *      Check the required condition, if the required condition is not met go to the label specified.
 *
 *  @param ASSERTION
 *      The condition that must be met before continue.
 *
 *  @param EXCEPTION_LABEL
 *      The label to go to when the required condition ASSERTION is not met.
 *
 *  @param ACTION
 *      The extra action to take before go to the EXCEPTION_LABEL label when ASSERTION is not met.
 *
 *  @discussion
 *      Example:
 *      require_action_quiet(
 *          foo == NULL, // required to be true
 *          exit, // if not met goto label
 *          ret = -1;  ERROR("foo should not be NULL") // before exiting
 *      ) ;
 */
#ifndef require_action_quiet
    #define require_action_quiet(ASSERTION, EXCEPTION_LABEL, ACTION)    \
        do {                                                            \
            if (__builtin_expect(!(ASSERTION), 0))                      \
            {                                                           \
                {                                                       \
                    ACTION;                                             \
                }                                                       \
                goto EXCEPTION_LABEL;                                   \
            }                                                           \
        } while(0)
#endif

/**
 *  @brief
 *      A macro that checks an assertion and jumps to an exception label if the assertion fails.
 *
 *  @param ASSERTION
 *      The condition that must evaluate to true (non-zero) for the macro to proceed normally.
 *
 *  @param EXCEPTION_LABEL
 *      A label in the current scope to which control will jump if the assertion fails.
 */
#ifndef require_quiet
    #define require_quiet(ASSERTION, EXCEPTION_LABEL)                   \
        do {                                                            \
            if (__builtin_expect(!(ASSERTION), 0))                      \
            {                                                           \
                goto EXCEPTION_LABEL;                                   \
            }                                                           \
        } while(0)
#endif

/**
 *  @brief
 *      Checks if an error code is zero and jumps to the provided `EXCEPTION_LABEL` if not.
 *
 *  @param ERR
 *      An error code that must be zero for the condition to pass.
 *
 *  @param EXCEPTION_LABEL
 *      The label to jump to in case of a non-zero error code.
 */
#define srp_require_noerr(ERR, EXCEPTION_LABEL) \
    require_quiet(((ERR) == 0), EXCEPTION_LABEL)

/*!
 *    @brief
 *        Returns from the current function if an expression evaluates to false.
 *
 *    @param EXPRESSION
 *        The expression.
 */
#define srp_require_return(EXPRESSION)  \
    do {                                \
        if (!(EXPRESSION)) {            \
            return;                     \
        }                               \
    } while (0)

/*!
 *  @brief
 *      Check the required condition, if the required condition is not met go to the label specified.
 *
 *  @param ASSERTION
 *      The condition that must be met before continue.
 *
 *  @param EXCEPTION_LABEL
 *      The label to go to when the required condition ASSERTION is not met.
 *
 *  @param ACTION
 *      The extra action to take before go to the EXCEPTION_LABEL label when ASSERTION is not met.
 *
 *  @discussion
 *      Example:
 *      require_action(
 *          foo == NULL, // required to be true
 *          exit, // if not met goto label
 *          ret = -1;  ERROR("foo should not be NULL") // before exiting
 *      ) ;
 */
#ifndef require_action
    #define require_action(ASSERTION, EXCEPTION_LABEL, ACTION)    \
        do {                                                            \
            if (__builtin_expect(!(ASSERTION), 0))                      \
            {                                                           \
                {                                                       \
                    ACTION;                                             \
                }                                                       \
                goto EXCEPTION_LABEL;                                   \
            }                                                           \
        } while(0)
#endif // #ifndef require_action

/*!
 *    @brief
 *        Assigns a value to a variable if the variable's address isn't NULL.
 *
 *    @param VARIABLE_ADDR
 *        The variable's address.
 *
 *    @param VALUE
 *        The value.
 */
#define srp_assign(VARIABLE_ADDR, VALUE)    \
    do {                                    \
        if (VARIABLE_ADDR) {                \
            *(VARIABLE_ADDR) = (VALUE);     \
        }                                   \
    } while (0)

/*!
 *  @brief
 *      Check the required condition, if the required condition is not met, do the ACTION. It is usually used as DEBUG macro.
 *
 *  @param ASSERTION
 *      The condition that must be met before continue.
 *
 *  @param ACTION
 *      The extra action to take when ASSERTION is not met.
 *
 *  @discussion
 *      Example:
 *      verify_action(
 *          foo == NULL, // required to be true
 *          ERROR("foo should not be NULL")  // action to take if required is false
 *      ) ;
 */
#undef verify_action
#define verify_action(ASSERTION, ACTION)                                \
    if (__builtin_expect(!(ASSERTION), 0)) {                            \
        ACTION;                                                         \
    }                                                                   \
    else do {} while (0)

// Print true or false based on boolean value:
    static inline const char *bool_str(bool tf) {
        if (tf) return "true";
        return "false";
    }


#ifdef __cplusplus
} // extern "C"
#endif

#ifndef THREAD_DEVKIT_ADK
// Object type external definitions
#define OBJECT_TYPE(x) extern int x##_created, x##_finalized, old_##x##_created, old_##x##_finalized;
#define NW_OBJECT_TYPE(x) OBJECT_TYPE(x)
#include "object-types.h"
#endif // !THREAD_DEVKIT_ADK

#endif // __SRP_H

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
