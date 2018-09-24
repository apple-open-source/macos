/*
clang++ -std=c++14 -o /tmp/fbblob fbblob.cpp -framework IOKit -framework CoreFoundation -Wall
*/

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOGraphicsLib.h>

#include <libgen.h>
#include <stdio.h>
#include <sysexits.h>

#include <cassert>
#include <utility>
#include <vector>

#include <mach/mach.h>
#include <mach/mach_error.h>

using std::vector;

namespace {

#define SAFE_CFRELEASE(p) \
    do { if (static_cast<bool>(p)) { CFRelease(p); p = NULL; } } while(0)
#define CFGETUTF8CPTR(s) CFStringGetCStringPtr((s), kCFStringEncodingUTF8)

const char *sCmdName = "whoami?";
mach_port_t sMasterPort = MACH_PORT_NULL;

auto kDependentIDKey = CFSTR("IOFBDependentID");
auto kDependentIndexKey = CFSTR("IOFBDependentIndex");

struct FBEntry {
    CFStringRef fPath = NULL;
    CFMutableDictionaryRef fProperties = NULL;
    io_connect_t fConnect = MACH_PORT_NULL;
    uint32_t fRegId = 0;
    uint32_t fDepRegId = 0;
    uint32_t fDepInd = 0;

    void close() {
        if (static_cast<bool>(fConnect)) {
            IOServiceClose(fConnect);
            fConnect = MACH_PORT_NULL;
        }
    }
    void clean() {
        close();
        SAFE_CFRELEASE(fPath);
        SAFE_CFRELEASE(fProperties);
    }
    void transfer(FBEntry &&other) {
        fPath = other.fPath;
        fProperties = other.fProperties;
        fConnect = other.fConnect;
        fRegId = other.fRegId;
        fDepRegId = other.fDepRegId;
        fDepInd = other.fDepInd;
        other.fPath = NULL;
        other.fProperties = NULL;
        other.fRegId = 0;
        fConnect = MACH_PORT_NULL;
    }

    FBEntry() { }
    ~FBEntry() { clean(); }
    FBEntry(FBEntry&& other) { transfer(std::move(other)); }
    FBEntry& operator=(FBEntry&& other)
    {
        if (this != &other) {
            clean();
            transfer(std::move(other));
        }
        return *this;
    }

    explicit FBEntry(io_object_t obj)
    {
        fPath = IORegistryEntryCopyPath(obj, kIOServicePlane);
        kern_return_t err = IORegistryEntryCreateCFProperties(
                      obj, &fProperties, /* allocator */ NULL, /* options */ 0);
        if (err) {
            fprintf(stderr,
                    "%s: Unable to create properties for '%s' : %s(%x)\n",
                    sCmdName, CFGETUTF8CPTR(fPath), mach_error_string(err),err);
            exit(EX_OSERR);
        }

        uint64_t u64 = 0;
        err = IORegistryEntryGetRegistryEntryID(obj, &u64);
        fRegId = static_cast<uint32_t>(u64);
        if (err) {
            fprintf(stderr,
                    "%s: Unable to get RegID for '%s' : %s(%x)\n",
                    sCmdName, CFGETUTF8CPTR(fPath), mach_error_string(err),err);
            exit(EX_OSERR);
        }

        CFNumberRef num = static_cast<CFNumberRef>(
                            CFDictionaryGetValue(fProperties, kDependentIDKey));
        CFNumberGetValue(num, kCFNumberSInt64Type, &u64);
        fDepRegId = static_cast<uint32_t>(u64);

        num = static_cast<CFNumberRef>(
                         CFDictionaryGetValue(fProperties, kDependentIndexKey));
        CFNumberGetValue(num, kCFNumberSInt64Type, &u64);
        fDepInd = static_cast<uint32_t>(u64);
    }

    kern_return_t open()
    {
        if (!static_cast<bool>(fPath))
            return kIOReturnBadArgument;

        const auto fbre = IORegistryEntryCopyFromPath(sMasterPort, fPath);
        if (!static_cast<bool>(fbre))
            return kIOReturnNotFound;

        io_connect_t fbshared;
        kern_return_t err = IOServiceOpen(
                fbre, mach_task_self(), kIOFBDiagnoseConnectType, &fbshared);
        IOObjectRelease(fbre);
        if (!err) {
            close();
            fConnect = fbshared;
        }
        return err;
    }

    kern_return_t restoreCoreDisplayBlob(IOIndex ind, void *blob, size_t n)
    {
        if (!static_cast<bool>(fConnect))
            return kIOReturnNotOpen;

        const uint64_t arglist[] = {
            /* Version      */ 1,
            /* Blob index   */ static_cast<uint64_t>(ind),
            /* Blob pointer */ reinterpret_cast<uintptr_t>(blob),
            /* Blob length  */ n,
        };
        const auto argcnt
            = static_cast<uint32_t>(sizeof(arglist) / sizeof(arglist[0]));
        return IOConnectCallScalarMethod(
                fConnect, 21, arglist, argcnt, NULL, NULL);
    }
};

vector<FBEntry> sFB;
void populate_fblist()
{
    CFDictionaryRef svcMatch = IOServiceMatching("IOFramebuffer");

    IOMasterPort(MACH_PORT_NULL, &sMasterPort);

    io_iterator_t iter = MACH_PORT_NULL;
    kern_return_t err
        = IOServiceGetMatchingServices(sMasterPort, svcMatch, &iter);
    svcMatch = NULL;  // Released by GetMatchingServices
    if (err) {
        fprintf(stderr,
                "%s: Unable to create IOFramebuffer iterator: %s(%x)\n",
                sCmdName, mach_error_string(err), err);
        exit(EX_OSERR);
    }

    vector<FBEntry> builtlist;
    do {
        builtlist.clear();  // Clear down builtlist for another go
        IOIteratorReset(iter);

        io_object_t obj;
        while ( (obj = IOIteratorNext(iter)) ) {
            builtlist.emplace_back(FBEntry(obj));
            // Done with registry entry for now
            IOObjectRelease(obj); obj = MACH_PORT_NULL;
        }
    } while(!IOIteratorIsValid(iter));
    IOObjectRelease(iter);

    sFB = std::move(builtlist);
}

int list(const int argc, const char * argv[])
{
    (void) argc; (void) argv;

    int i = 0;
    for (const auto& entry : sFB) {
        printf("%2d[%3x]: %3x[%d] %s\n",
               i, entry.fRegId, entry.fDepRegId, entry.fDepInd,
               CFGETUTF8CPTR(entry.fPath));
        ++i;
    }
    return EX_OK;
}

int restore(const int argc, const char * argv[])
{
    vector<FBEntry>::size_type fbind = 0;
    IOIndex blobIndex = 0;

    if (argc >= 3)
        fbind = strtol(argv[2], NULL, 10);
    if (argc == 4)
        blobIndex = strtol(argv[3], NULL, 10);

    FBEntry& fb = sFB.at(fbind);
    kern_return_t err = fb.open();
    if (err) {
        fprintf(stderr, "%s: Unable to open fb[%d]: %s{%x}\n",
                sCmdName, static_cast<int>(fbind), mach_error_string(err), err);
        return EX_NOINPUT;
    }
    int ret = EX_OK;
    void *blobData = valloc(4096);
    err = fb.restoreCoreDisplayBlob(blobIndex, blobData, 4096);
    if (err) {
        fprintf(stderr, "%s: fb[%d] unable to restore blob: %s{%x}\n",
                sCmdName, static_cast<int>(fbind), mach_error_string(err), err);
        ret = EX_OSERR;
    }
    else
        fwrite(blobData, 4096, 1, stdout);
    free(blobData);
    fb.close();

    return ret;
}

int update(const int argc, const char * argv[])
{
    vector<FBEntry>::size_type fbind = 0;
    IOIndex blobIndex = 0;

    if (argc >= 3)
        fbind = strtol(argv[2], NULL, 10);
    if (argc == 4)
        blobIndex = strtol(argv[3], NULL, 10);

    FBEntry& fb = sFB.at(fbind);
    kern_return_t err = fb.open();
    if (err) {
        fprintf(stderr, "%s: Unable to open fb[%d]: %s{%x}\n",
                sCmdName, static_cast<int>(fbind), mach_error_string(err), err);
        return EX_NOINPUT;
    }
    int ret = EX_OK;
    void *blobData = valloc(4096);
    err = fb.restoreCoreDisplayBlob(blobIndex, blobData, 4096);
    if (err) {
        fprintf(stderr, "%s: fb[%d] unable to update blob: %s{%x}\n",
                sCmdName, static_cast<int>(fbind), mach_error_string(err), err);
        ret = EX_OSERR;
    }
    else
        fread(blobData, 4096, 1, stdin);
    free(blobData);
    fb.close();

    return ret;
}

bool startswith(const char *str, const size_t sl, const char *arg)
{
    const size_t al = strlen(arg);
    const size_t len = (sl < al) ? sl : al;
    return 0 == strncmp(str, arg, len);
}
#define STARTSWITH(str, arg) startswith(str, (sizeof(str)-1), arg)
}; // namespace

int main(const int argc, char * argv[])
{
    sCmdName = basename(argv[0]);
    const char * const verb = argv[1];
    const char ** cargv = const_cast<const char **>(argv);

    populate_fblist();

    // verb decode
    if (STARTSWITH("list", verb))
        return list(argc, cargv);
    else if (STARTSWITH("restore", verb))
        return restore(argc, cargv);
    else if (STARTSWITH("update", verb))
        return update(argc, cargv);
    else {
        fprintf(stderr, "%s: Unknown verb '%s'\n", sCmdName, verb);
        return EX_USAGE;
    }
}
