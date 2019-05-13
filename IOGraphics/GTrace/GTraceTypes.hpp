//
//  GTraceTypes.hpp
//  IOGraphics
//
//  Created by Jeremy Tran on 8/8/17.
//  Rewritten by Godfrey van der Linden on 2018-08-17
//  Shared file between kernel and user land
//

#ifndef GTraceTypes_hpp
#define GTraceTypes_hpp

#include <stdint.h>
#include <os/base.h>

#include <string.h>

#ifndef GTRACE_ARCHAIC_CPP
#define GTRACE_ARCHAIC_CPP (__cplusplus < 201103L)
#endif

#define GTRACE_REVISION         0x2

#define kGTraceMaximumBufferCount 32

#pragma mark - Masks
#define kGTRACE_COMPONENT_MASK      0x00000000FFFFFFFFULL       // 32 bits
#define kTHREAD_ID_MASK             0x0000000000FFFFFFULL       // 24 bits
#define kGTRACE_REGISTRYID_MASK     0x00000000FFFFFFFFULL       // 32 bits


// Argument Tag Bits, might be better as a 16 value(4 bit) field, but for the
// time being lets not confuse the binary interpretation of existing data.
#define kGTRACE_ARGUMENT_Reserved1   0x1000  // Future Use
#define kGTRACE_ARGUMENT_STRING      0x2000  // Argument string may be swapped to Host byte order
#define kGTRACE_ARGUMENT_BESTRING    0x4000  // Argument string is in big endian byte order
#define kGTRACE_ARGUMENT_POINTER     0x8000  // Arguments tagged with this bit are pointers and will be obfuscated when copied/printed. Must not be used in the first ARG position

#define kGTRACE_ARGUMENT_STRING_MASK (kGTRACE_ARGUMENT_STRING | kGTRACE_ARGUMENT_BESTRING)
#define kGTRACE_ARGUMENT_MASK        0xF000

// Helpers
#define MAKEGTRACETAG(x) static_cast<uint16_t>(x)
#define MAKEGTRACEARG(x) static_cast<uint64_t>(x)

#define GMUL8(v)                     ((v) << 3)
#define GMUL16(v)                    ((v) << 4)
#define GMUL32(v)                    ((v) << 5)
#define GPACKNODATA                                         0                           // 0 is a valid value, NoData used to distinguish
#define GPACKBITS(shift, value) \
    (MAKEGTRACEARG(value) & ((1ULL << (shift)) - 1))

/* GPACKBIT
 * value: boolean value
 * bitidx: index of bit in uint64_t result; valid range: [0, 63] */
#define GPACKBIT(bitidx, value) \
    MAKEGTRACEARG((static_cast<bool>(value)) ? (1ULL << (bitidx)) : 0)
#define GUNPACKBIT(bitidx, value) \
    static_cast<bool>(MAKEGTRACEARG(value) & (1ULL << (bitidx)))

/* GPACKUINT8T
 * value: uint8_t value
 * ui8idx: index of uint8_t in uint64_t result; valid range: [0, 7] */
#define GPACKUINT8T(ui8idx,   value) \
    (MAKEGTRACEARG(value & 0x000000ff) << GMUL8(ui8idx))
#define GUNPACKUINT8T(ui8idx, value) \
    static_cast<uint8_t>(MAKEGTRACEARG(value) >> GMUL8(ui8idx))

/* GPACKUINT16T
 * value: uint16_t value
 * ui16idx: index of uint16_t in uint64_t result; valid range: [0, 3] */
#define GPACKUINT16T(ui16idx, value) \
    (MAKEGTRACEARG(value & 0x0000ffff) << GMUL16(ui16idx))
#define GUNPACKUINT16T(ui16idx, value) \
    static_cast<uint16_t>(MAKEGTRACEARG(value) >> GMUL16(ui16idx))

/* GPACKUINT32T
 * value: uint32_t value
 * ui8idx: index of uint32_t in uint64_t result; valid range: [0, 1] */
#define GPACKUINT32T(ui32idx, value) \
    (MAKEGTRACEARG(value & 0xffffffff) << GMUL32(ui32idx))
#define GUNPACKUINT32T(ui32idx, value) \
    static_cast<uint32_t>(MAKEGTRACEARG(value) >> GMUL32(ui32idx))

#define GPACKUINT64T(value)    (value)
#define GUNPACKUINT64T(value)  (value)

#define GPACKSTRINGCAST(ia)  reinterpret_cast<char *>(ia)
#define GPACKSTRING(ia, str) strlcpy(GPACKSTRINGCAST(ia), str, sizeof(ia))

// User client commands
OS_ENUM(gtrace_command, uint64_t,
    kGTraceCmdFetch = 'Ftch',
);

// These structures must be power of 2 for performance, alignment, and the
// buffer-to-line calculations
#pragma pack(push, 1)

struct GTraceEntry {
    // Internal structures
    struct ID {
        uint64_t fID;

#if !GTRACE_ARCHAIC_CPP
        ID(uint64_t id) : fID(id) {}
        ID(uint64_t line, uint64_t component)
            : fID((line & 0xffff) | (component << 16)) {}
#endif

        uint64_t id() const        { return fID; }
        uint16_t line() const      { return static_cast<uint16_t>(fID); }
        uint64_t component() const { return static_cast<uint64_t>(fID) >> 16; }
    };

    struct ThreadInfo {
        uint64_t fTi;

#if !GTRACE_ARCHAIC_CPP
        ThreadInfo(uint64_t cpu, uint64_t threadID, uint64_t registryID)
            : fTi( (cpu & 0xff) | ((threadID & 0xffffff) << 8)
                 | ((registryID & 0xffffffff) << 32)) {}
#endif

        uint8_t  cpu() const        { return static_cast<uint8_t>(fTi); }
        uint32_t threadID() const   { return static_cast<uint32_t>(fTi) >> 8; }
        uint32_t registryID() const { return static_cast<uint32_t>(fTi >> 32); }
    };

    struct ArgsTag {
        static const int kNum = 4;
        union {
            uint16_t    fTarg[kNum];
            uint64_t    fTag64;
        };

#if !GTRACE_ARCHAIC_CPP
        ArgsTag(uint64_t tag) : fTag64{tag} {}
        ArgsTag(uint16_t tag0, uint16_t tag1, uint16_t tag2, uint16_t tag3)
        {
            fTarg[0] = tag0; fTarg[1] = tag1; fTarg[2] = tag2; fTarg[3] = tag3;
        }
#endif

        uint64_t tag() const                     { return fTag64;     }
        const uint16_t& tag(const int idx) const { return fTarg[idx]; }
        uint16_t& tag(const int idx)             { return fTarg[idx]; }
    };

    struct Args {
        union {
            uint64_t    fU64s[4];
            uint32_t    fU32s[8];
            uint16_t    fU16s[16];
            uint8_t     fU8s[32];
            char        fStr[32];
        };

#if !GTRACE_ARCHAIC_CPP
        Args(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3)
        {
            fU64s[0] = arg0; fU64s[1] = arg1; fU64s[2] = arg2; fU64s[3] = arg3;
        }
#endif

        const uint64_t& u64(const int idx) const { return fU64s[idx]; }
        const uint32_t& u32(const int idx) const { return fU32s[idx]; }
        const uint16_t& u16(const int idx) const { return fU16s[idx]; }
        const uint8_t&  u8(const int idx)  const { return fU8s[idx];  }
        const char* str()                  const { return fStr; }
        uint64_t& u64(const int idx)             { return fU64s[idx]; }
        uint32_t& u32(const int idx)             { return fU32s[idx]; }
        uint16_t& u16(const int idx)             { return fU16s[idx]; }
        uint8_t&  u8(const int idx)              { return fU8s[idx];  }
        char* str()                              { return fStr; }
    };

    // GTraceEntry data
    union {
        struct {
            uint64_t   fTimestamp;      // mach continuous time
            ID         fID;             // unique ID to entry
            ThreadInfo fThreadInfo;     // CPU, thread info
            ArgsTag    fArgsTag;        // Argument tags
            Args       fArgs;           // Argument data
        };
#if !GTRACE_ARCHAIC_CPP
        uint64_t       fEntry[8] = { 0 };
#else
        uint64_t       fEntry[8];
#endif
    };

#if !GTRACE_ARCHAIC_CPP
    GTraceEntry() {}

    // Special entry used as the first entry in a binary GTrace file
    // If the timestamp/fEntry[0] == -1, then fEntry[1] contains a count of the
    // number of buffers in this file.  The buffers themselves are self
    // describing and contain version information.
    GTraceEntry(const int16_t numBuffers, const uint64_t gtraceVersion)
    {
        memset(&fEntry[0], -1, sizeof(fEntry));
        fID = ID(numBuffers, gtraceVersion);
    }

    // Buffer bound Entry, inserted during sorting for output
    GTraceEntry(const uint64_t timestamp, const uint64_t component,
                const uint16_t funcid)
    {
        fTimestamp = timestamp;
        fID = ID{0, component};
        fArgsTag.fTarg[0] = funcid;
    }

    GTraceEntry(const uint64_t timestamp,   const uint16_t line,
                const uint64_t componentID, const uint8_t  cpu,
                const uint64_t tID,         const uint32_t regID,
                const uint16_t tag1,        const uint64_t arg1,
                const uint16_t tag2,        const uint64_t arg2,
                const uint16_t tag3,        const uint64_t arg3,
                const uint16_t tag4,        const uint64_t arg4)
        : fTimestamp{timestamp}, fID{line, componentID},
          fThreadInfo{cpu, tID, regID},
          fArgsTag{tag1, tag2, tag3, tag4}, fArgs{arg1, arg2, arg3, arg4} {}
#endif // !GTRACE_ARCHAIC_CPP

    GTraceEntry(const uint64_t timestamp,   const uint16_t line,
                const uint64_t componentID, const uint8_t  cpu,
                const uint64_t tID,         const uint32_t regID,
                const uint64_t tags,        const uint64_t arg1,
                const uint64_t arg2,        const uint64_t arg3,
                const uint64_t arg4)
        : fTimestamp{timestamp}, fID{line, componentID},
          fThreadInfo{cpu, tID, regID},
          fArgsTag{tags}, fArgs{arg1, arg2, arg3, arg4} {}

    // Copy constructor and assignment
    GTraceEntry(const GTraceEntry& other)
        { memcpy(&fEntry[0], &other.fEntry[0], sizeof(fEntry)); }
    GTraceEntry& operator=(const GTraceEntry& other)
    {
        memcpy(&fEntry[0], &other.fEntry[0], sizeof(fEntry));
        return *this;
    }

    // Accessors
    uint64_t timestamp() const          { return fTimestamp;               }
    uint64_t id() const                 { return fID.id();                 }
    uint16_t line() const               { return fID.line();               }
    uint64_t component() const          { return fID.component();          }
    uint8_t  cpu() const                { return fThreadInfo.cpu();        }
    uint32_t threadID() const           { return fThreadInfo.threadID();   }
    uint32_t registryID() const         { return fThreadInfo.registryID(); }
    uint64_t tag() const                { return fArgsTag.tag(); }
    const uint16_t& tag(const int idx) const   { return fArgsTag.tag(idx); }
    const uint64_t& arg64(const int idx) const { return fArgs.u64(idx);    }
    const uint32_t& arg32(const int idx) const { return fArgs.u32(idx);    }
    const uint16_t& arg16(const int idx) const { return fArgs.u16(idx);    }
    const uint8_t & arg8(const int idx)  const { return fArgs.u8(idx);     }
    uint16_t& tag(const int idx)   { return fArgsTag.tag(idx); }
    uint64_t& arg64(const int idx) { return fArgs.u64(idx);    }
    uint32_t& arg32(const int idx) { return fArgs.u32(idx);    }
    uint16_t& arg16(const int idx) { return fArgs.u16(idx);    }
    uint8_t & arg8(const int idx)  { return fArgs.u8(idx);     }

    // For sorting, timestamp, ID, CPUThreadID as uint64_t
    friend bool operator<(const GTraceEntry& lhs, const GTraceEntry& rhs);
};
#define kGTraceEntrySize static_cast<int>(sizeof(GTraceEntry))

struct GTraceHeader
{                                // [ind] uint64_ts
    uint64_t fDecoderName[4];    //   0   32 byte decoder module name
    uint64_t fBufferName[4];     //   4   32 byte buffer name
    uint64_t fCreationTime;      //   8   creation mach continuous time
    uint32_t fBufferID;          //   9   Unique ID of buffer
    uint32_t fBufferSize;        //   9   Bytes, buffer size including header
    uint32_t fTokenLine;         //  10   Current out token
    uint16_t fVersion;           //  10   GTrace version
    uint16_t fBufferIndex;       //  10   Index of buffer in system
    uint16_t fTokensMask;        //  11   Mask from fTokenLine to index
    uint16_t fBreadcrumbTokens;  //  11   Size in tokens of breadcrumb
    uint16_t fTokensCopied;      //  11   Number of fTokens copied
};

// Note every section is a multiple of sizeof(GTraceEntry), i.e. 64, bytes.
#define kGTraceHeaderEntries 2
#define kGTraceHeaderSize (kGTraceHeaderEntries * kGTraceEntrySize)
struct IOGTraceBuffer
{
    union {
        GTraceHeader fHeader;
        GTraceEntry _padding[kGTraceHeaderEntries];
    };
    // Breadcrumb, fTokens[0]–fTokens[fHeader.fBreadcrumbTokens] inclusive
    // Tokens, fTokens[fHeader.fBreadcrumbTokens]–fTokens[fHeader.fTokensCopied]
    GTraceEntry fTokens[];
};

#pragma pack(pop)

// May need to redesign if this assumption changes, test for it
static_assert(sizeof(GTraceHeader) <= kGTraceHeaderSize,
    "header doesnt fit in two entries, change union to preserve alignment");
static_assert(kGTraceEntrySize == 8 * sizeof(uint64_t),
    "GTraceEntry != 64 bytes");

#if !KERNEL
#include <vector>
#include <utility> // for std::pair<T1, T2>
class GTraceBuffer
{
public:
    using vector_type = std::vector<GTraceEntry>;
    using vector_iter_type = vector_type::iterator;
    using vector_citer_type = vector_type::const_iterator;

    using breadcrumb_type = std::pair<void*, size_t>;
    using tokens_type = std::pair<vector_iter_type, vector_iter_type>;
    using ctokens_type = std::pair<vector_citer_type, vector_citer_type>;

    explicit GTraceBuffer(vector_type&& entries)
        : fData(std::move(entries))
    {}
    GTraceBuffer(vector_citer_type begin, vector_citer_type end)
        : fData(vector_type(begin, end))
    {}

    // Move constructor and assignment
    GTraceBuffer(GTraceBuffer&& other)
        : fData(std::move(other.fData))
    {}
    GTraceBuffer& operator=(GTraceBuffer&& other)
    {
        if (this != &other)
            fData = std::move(other.fData);
        return *this;
    }

    // copy constructor and assignment
    GTraceBuffer(const GTraceBuffer& other)            = delete;
    GTraceBuffer& operator=(const GTraceBuffer& other) = delete;

    // Accessors
    GTraceHeader& header()
        { return *reinterpret_cast<GTraceHeader*>(fData.data()); }
    const GTraceHeader& header() const
        { return *reinterpret_cast<const GTraceHeader*>(fData.data()); }

    breadcrumb_type breadcrumb()
    {
        const size_t bcl = header().fBreadcrumbTokens;
        auto& firstBCEntry = fData.at(2);
        void* bcP = reinterpret_cast<void*>(&firstBCEntry);
        return breadcrumb_type{(bcl ?  bcP : nullptr), bcl};
    }
    tokens_type tokens()
    {
        tokens_type ret{fData.begin(), fData.begin()};  // Zero length
        if (header().fTokensCopied) {
            const uint32_t& bct = header().fBreadcrumbTokens;
            ret = tokens_type{fData.begin() + 2 + bct, fData.end()};
        }
        return ret;
    }
    ctokens_type ctokens() const
    {
        ctokens_type ret{fData.cend(), fData.cend()};  // Zero length
        if (header().fTokensCopied) {
            const uint32_t& bct = header().fBreadcrumbTokens;
            ret = ctokens_type{fData.cbegin() + 2 + bct, fData.cend()};
        }
        return ret;
    }
    const vector_type& vec() const      { return fData; }
    const GTraceEntry* data() const     { return fData.data(); }
    vector_type::size_type size() const { return fData.size(); }

    void removeTokens()
    {
        ctokens_type ctokenpair = ctokens();
        fData.erase(ctokenpair.first, ctokenpair.second);
        fData.shrink_to_fit();
    }

private:
    std::vector<GTraceEntry> fData;
};
#endif // !KERNEL
#endif /* GTraceTypes_hpp */
