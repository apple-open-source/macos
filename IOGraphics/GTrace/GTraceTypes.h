//
//  GTraceTypes.h
//  IOGraphics
//
//  Created by Jeremy Tran on 8/8/17.
//

#ifndef GTrace_h
#define GTrace_h

#include <stdint.h>
#include <sys/cdefs.h>

#pragma mark - Header Revision
#define         GTRACE_REVISION                             0x1

#pragma mark - Constants
#define         kGTraceMaximumLineCount                     8192    // @64b == 512k
#define         kGTraceDefaultLineCount                     2048    // @64b == 128K

__BEGIN_DECLS

#pragma mark - GTrace Structures
/*
 These structures must be power of 2 for performance, alignment, and the buffer-to-line calculations
 */
#pragma pack(push, 1)
typedef struct _GTraceID {
    union {
        struct {
            uint64_t    line:16;
            uint64_t    component:48;
        };
        uint64_t    u64;
    } ID;
} sGTraceID;

typedef struct _GTraceThreadInfo {
    union {
        struct {
            uint64_t    cpu:8;
            uint64_t    threadID:24;
            uint64_t    registryID:32;
        };
        uint64_t    u64;
    } TI;
} sGTraceThreadInfo;

typedef struct _GTraceArgsTag {
    union {
        struct {
            uint16_t    targ[4];
        };
        uint64_t    u64;
    } TAG;
} sGTraceArgsTag;

typedef struct _GTraceArgs {
    union {
        uint64_t    u64s[4];
        uint32_t    u32s[8];
        uint16_t    u16s[16];
        uint8_t     u8s[32];
        char        str[32];
    } ARGS;
} sGTraceArgs;

typedef struct _GTrace {
    union {
        struct {
            uint64_t            timestamp;      // mach absolute time
            sGTraceID           traceID;        // unique ID to entry
            sGTraceThreadInfo   threadInfo;     // CPU, thread info
            sGTraceArgsTag      argsTag;        // Argument tags
            sGTraceArgs         args;           // Argument data
        };
        uint64_t            entry[8];
    } traceEntry;
} sGTrace;
#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(sGTrace) == 64, "sGTrace != 64 bytes");
#else
#include <AssertMacros.h>
__Check_Compile_Time(sizeof(sGTrace) == 64);
#endif

__END_DECLS

#endif /* GTraceTypes_h */
