/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>

#if !TARGET_OS_EXCLAVEKIT

#include <atomic>
#include <assert.h>

#include <mach/mach.h>
#include <kern/kcdata.h>
#include <mach-o/dyld_priv.h>
#include <mach-o/dyld_images.h>
#include <sys/fsgetpath.h>

#include "Tracing.h"
#include "DyldRuntimeState.h"

// Workaround for header issues in rdar://49073930
// #include <System/os/reason_private.h>
extern "C" int
os_fault_with_payload(uint32_t reason_namespace, uint64_t reason_code,
                      void *payload, uint32_t payload_size, const char *reason_string,
                      uint64_t reason_flags) __attribute__((cold));

#if BUILDING_DYLD
extern struct dyld3::MachOAnalyzer __dso_handle;
#endif

namespace dyld3 {

#if BUILDING_DYLD
static
void kdebug_trace_dyld_region(const uint32_t code, cpu_type_t cputype, cpu_subtype_t cpuSubtype, const char* imagePath,
                             const uuid_t* uuid_bytes, const fsobj_id_t fsobjid, const fsid_t fsid, const void* load_addr)
{
    uint64_t id = kdebug_trace_string(KDBG_CODE(DBG_DYLD, DBG_DYLD_UUID, code), 0, imagePath);
    uint64_t *uuid = (uint64_t *)uuid_bytes;
    kdebug_trace(KDBG_CODE(DBG_DYLD, DBG_DYLD_UUID, code), uuid[0],
                 uuid[1], (uint64_t)load_addr,
                 (uint64_t)fsid.val[0] | ((uint64_t)fsid.val[1] << 32));
    kdebug_trace(KDBG_CODE(DBG_DYLD, DBG_DYLD_UUID, code + 1),
                 (uint64_t)fsobjid.fid_objno |
                 ((uint64_t)fsobjid.fid_generation << 32),
                 id, ((uint64_t)cputype << 32) | cpuSubtype, 0);
    kdebug_trace_string(KDBG_CODE(DBG_DYLD, DBG_DYLD_UUID, code), id, nullptr);
}
#endif // BUILDING_DYLD

VIS_HIDDEN
void kdebug_trace_dyld_image(const uint32_t code,
                             const char* imagePath,
                             const uuid_t* uuid_bytes,
                             const fsobj_id_t fsobjid,
                             const fsid_t fsid,
                             const void* load_addr,
                             uint32_t cpusubtype)
{
#if BUILDING_DYLD
    // note, assumes image cputype must match dyld
    kdebug_trace_dyld_region(code, __dso_handle.cputype, cpusubtype, imagePath, uuid_bytes, fsobjid, fsid, (const void*)load_addr);
#endif
}

void kdebug_trace_dyld_cache(uint64_t rawFSObjID, uint64_t rawFSID, uint64_t sharedCacheBaseAddress,
                             const uint8_t sharedCacheUUID[16])
{
#if BUILDING_DYLD
    if ( !dyld3::kdebug_trace_dyld_enabled(KDBG_CODE(DBG_DYLD, DBG_DYLD_UUID, DBG_DYLD_UUID_SHARED_CACHE_A)) )
        return;

    // Don't send the event if dyld is in the cache.  In this case, we already sent the event when we transitioned to dyld in the cache
    if ( __dso_handle.inDyldCache() )
        return;

    fsid_t*     fsid    = (fsid_t*)&rawFSID;
    fsobj_id_t* fsobjid = (fsobj_id_t*)&rawFSObjID;

    char pathBuffer[PATH_MAX] = { 0 };
    // TODO: Add the shared cache path to the dynamic config region so that we don't need this call
    if ( fsgetpath(pathBuffer, PATH_MAX, fsid, rawFSObjID) == -1 ) {
        // Failed, likely due to the sandbox.  Zero the buffer for now.
        pathBuffer[0] = 0;
    }

    kdebug_trace_dyld_region(DBG_DYLD_UUID_SHARED_CACHE_A, 0, 0, pathBuffer, (const uuid_t*)&sharedCacheUUID[0],
                             *fsobjid, *fsid, (const void*)sharedCacheBaseAddress);
#endif // BUILDING_DYLD
}

// FIXME
// We get distinct copies of this in libdyld and dyld. Eventually we can fix it,
// for now we will just offset the values.
#if BUILDING_DYLD
static std::atomic<uint64_t> trace_pair_id(0);
#else
static std::atomic<uint64_t> trace_pair_id(1LL<<63);
#endif

VIS_HIDDEN
bool kdebug_trace_dyld_enabled(uint32_t code) {
    return kdebug_is_enabled(code);
}

VIS_HIDDEN
void kdebug_trace_dyld_marker(uint32_t code, kt_arg data1, kt_arg data2, kt_arg data3, kt_arg data4) {
    if (kdebug_is_enabled(code)) {
        data1.prepare(code);
        data2.prepare(code);
        data3.prepare(code);
        data4.prepare(code);
        kdebug_trace(code, data1.value(), data2.value(), data3.value(), data4.value());
        data4.destroy(code);
        data3.destroy(code);
        data2.destroy(code);
        data1.destroy(code);
    }
}

VIS_HIDDEN
uint64_t kdebug_trace_dyld_duration_start(uint32_t code, kt_arg data1, kt_arg data2, kt_arg data3) {
    uint64_t result = 0;
    if (kdebug_is_enabled(code)) {
        result = ++trace_pair_id;
        data1.prepare(code);
        data2.prepare(code);
        data3.prepare(code);
        kdebug_trace(code | DBG_FUNC_START, result, data1.value(), data2.value(), data3.value());
        data3.destroy(code);
        data2.destroy(code);
        data1.destroy(code);
    }
    return result;
}

VIS_HIDDEN
void kdebug_trace_dyld_duration_end(uint64_t trace_id, uint32_t code, kt_arg data1, kt_arg data2, kt_arg data3) {
    if (trace_id != 0 && kdebug_is_enabled(code)) {
        data1.prepare(code);
        data2.prepare(code);
        data3.prepare(code);
        kdebug_trace(code | DBG_FUNC_END, trace_id, data1.value(), data2.value(), data3.value());
        data3.destroy(code);
        data2.destroy(code);
        data1.destroy(code);
    }
}

void ScopedTimer::startTimer() {
    current_trace_id = kdebug_trace_dyld_duration_start(code, data1, data2, data3);
}

void ScopedTimer::endTimer() {
    kdebug_trace_dyld_duration_end(current_trace_id, code, data4, data5, data6);
}

#if 0
void syntheticBacktrace(const char *reason, bool enableExternally) {
    if (!enableExternally && !config.internalInstall()) { return; }
    char payloadBuffer[EXIT_REASON_PAYLOAD_MAX_LEN];
    dyld_abort_payload* payload = (dyld_abort_payload*)payloadBuffer;
    payload->version               = 1;
    payload->flags                 = 0;
    payload->targetDylibPathOffset = 0;
    payload->clientPathOffset      = 0;
    payload->symbolOffset          = 0;
    int payloadSize = sizeof(dyld_abort_payload);
    char truncMessage[EXIT_REASON_USER_DESC_MAX_LEN];
    strlcpy(truncMessage, reason, EXIT_REASON_USER_DESC_MAX_LEN);
    os_fault_with_payload(OS_REASON_DYLD, DYLD_EXIT_REASON_OTHER, payloadBuffer, payloadSize, truncMessage, 0);
}
#endif

};

#endif // !TARGET_OS_EXCLAVEKIT
