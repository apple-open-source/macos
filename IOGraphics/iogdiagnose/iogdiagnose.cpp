//
//  iogdiagnose.cpp
//  iogdiagnose
//
//  Created by bparke on 12/16/16.
//
//
/*
 State bits documentation:

 OP: Opened (bool)
 ON: Online (bool)
 US: Usable (bool)
 PS: Paging (bool)
 CS: Clamshell (bool)
 CC: Clamshell Current (bool)
 CL: Clamshell Last (bool)
 DS: System Dark (bool)
 MR: Mirrored (bool)
 SG: System Gated (bool)
 WL: IOG Workloop Gated (bool)
 NA: FB Notifications Active (bool)
 SN: WindowServer (CoreDisplay) Notified (bool)
 SS: WindowServer (CoreDisplay) Notification State (bool)
 SA: WindowServer (CoreDisplay) Pending Acknowledgement (bool)
 PP: Pending Power Change (bool)
 SP: System Power AckTo (bool)
 MS: Mux Switching (bool)               // version 6+
 MP: Mux Power Change Pending (bool)    // version 6+
 MU: Muted (bool)                       // version 6+
 MX: Pending Mux Change (bool)          // version 5-
 PPS: Pending Power State (ordinal)
 NOTIFIED: Active FB Notification Group ID (ID)
 WSAA: Active WSAA State (enum/bitfield)

 A-State (External API-State)

 CreateSharedCursor                 0x00 00 00 01
 GetPixelInformation                0x00 00 00 02
 GetCurrentDisplayMode              0x00 00 00 04
 SetStartupDisplayMode              0x00 00 00 08

 SetDisplayMode                     0x00 00 00 10
 GetInformationForDisplayMode       0x00 00 00 20
 GetDisplayModeCount                0x00 00 00 40
 GetDisplayModes                    0x00 00 00 80

 GetVRAMMapOffset                   0x00 00 01 00
 SetBounds                          0x00 00 02 00
 SetNewCursor                       0x00 00 04 00
 SetGammaTable                      0x00 00 08 00

 SetCursorVisible                   0x00 00 10 00
 SetCursorPosition                  0x00 00 20 00
 AcknowledgeNotification            0x00 00 40 00
 SetColorConvertTable               0x00 00 80 00

 SetCLUTWithEntries                 0x00 01 00 00
 ValidateDetailedTiming             0x00 02 00 00
 GetAttribute                       0x00 04 00 00
 SetAttribute                       0x00 08 00 00

 WSStartAttribute                   0x00 10 00 00
 EndConnectionChange                0x00 20 00 00
 ProcessConnectionChange            0x00 40 00 00
 RequestProbe                       0x00 80 00 00

 Close                              0x01 00 00 00
 SetProperties                      0x02 00 00 00
 SetHibernateGamma                  0x04 00 00 00

 */

// C++ headers
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

extern char **environ;


#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/wait.h>

#include "IOGDiagnoseUtils/iokit"
#include "IOGDiagnoseUtils/IOGDiagnoseUtils.hpp"

#include "GTrace/GTraceTypes.hpp"

#define kFILENAME_LENGTH                64

using std::string;
using std::vector;

namespace {

void print_usage(const char* name)
{
    fprintf(stdout, "%s options:\n", name);

    fprintf(stdout, "\t--file | -f\n");
    fprintf(stdout, "\t\tWrite diag to file /tmp/com.apple.iokit.IOGraphics_YYYY_MM__DD_HH-MM-SS.txt (instead of stdout)\n");

    fprintf(stdout, "\n\t--binary | -b binary_file\n");

    fprintf(stdout, "\n");
    fflush(stdout);
}

void foutsep(FILE* outfile, const string title)
{
    static const char kSep[] = "---------------------------------------------------------------------------------------";
    static const int kSepLen = static_cast<int>(sizeof(kSep) - 1);
    if (title.empty()) {
        fprintf(outfile, "\n%s\n", kSep);
        return;
    }
    // Add room for spaces
    const int titlelen = static_cast<int>(title.length()) + 2;
    assert(titlelen < kSepLen);
    const int remainsep = kSepLen - titlelen;
    const int prefixsep = remainsep / 2;
    const int sufficsep = prefixsep + (remainsep & 1);

    fprintf(outfile, "%.*s %s %.*s\n\n", 
            prefixsep, kSep, title.c_str(), sufficsep, kSep);
}
inline void foutsep(FILE* outfile) { foutsep(outfile, string()); }

string stringf(const char *fmt, ...)
{
    va_list args;
    char buffer[1024]; // pretty big, but userland stacks are big

    va_start(args, fmt);
    const auto len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    assert(len < sizeof(buffer)); (void) len;
    return buffer;
}

string formatEntry(const int currentLine, const GTraceEntry& entry)
{
    return stringf(
            "\t\tTkn: %06u\tTS: %llu\tLn: %u\tC: %#llx\tCTID: %u-%#llx\t"
            "OID: %#llx\tTag: %#llx\tA: %#llx-%#llx-%#llx-%#llx\n",
            currentLine, entry.timestamp(), entry.line(), entry.component(),
            entry.cpu(), entry.threadID(), entry.registryID(),
            entry.tag(), entry.arg64(0), entry.arg64(1),
            entry.arg64(2), entry.arg64(3));
}

void agdcdiagnose(FILE * outfile)
{
#define AGCKEXT      "/System/Library/Extensions/AppleGraphicsControl.kext/"
#define BNDLBINDIR   "Contents/MacOS/"
#define AGDCDIAGEXEC "AGDCDiagnose"
    static const char* kDiagArgs[]
        = { AGCKEXT BNDLBINDIR AGDCDIAGEXEC, "-a", nullptr };

    if (static_cast<bool>(outfile)) {
        pid_t       pid = 0;
        foutsep(outfile, "AGDC REPORT"); fflush(outfile);

        auto agdcArgs = const_cast<char* const *>(kDiagArgs);
        int status = posix_spawn(&pid, kDiagArgs[0], nullptr, nullptr,
                                 agdcArgs, environ);
        if (0 == status)
            (void) waitpid(pid, &status, 0);
        else
            fprintf(outfile, "\tAGDCDiagnose failed to launch\n");
        foutsep(outfile);
    }
}

void dumpTokenBuffer(FILE* outfile, const vector<GTraceBuffer>& gtraces)
{
    unsigned currentLine = 0;
    long total_lines = 1; // Include magic
    for (const GTraceBuffer& buffer : gtraces)
        total_lines += buffer.vec().size();
    if (!total_lines) {
        fprintf(outfile, "-1 (No Token Data Recorded)\n\n");
        return;
    }

    // Marks beginning of a multi
    const GTraceEntry magic(gtraces.size(), GTRACE_REVISION);
    string line = formatEntry(currentLine++, magic);

    fputs("\n\n", outfile);
    fprintf(outfile, "Token Buffers Recorded: %d\n",
            static_cast<int>(gtraces.size()));
    fprintf(outfile, "Token Buffer Lines    : %ld\n", total_lines);
    fprintf(outfile, "Token Buffer Size     : %ld\n",
            total_lines * kGTraceEntrySize);
    fprintf(outfile, "Token Buffer Data     :\n");
    fputs(line.c_str(), outfile); // out magic marks as v2 or later

    for (const GTraceBuffer& buffer : gtraces) {
        for (const GTraceEntry& entry : buffer.vec()) {
            line = formatEntry(currentLine++, entry);
            fputs(line.c_str(), outfile);
        }
    }
    fputc('\n', outfile);
}

void writeGTraceBinary(const vector<GTraceBuffer>& gtraces,
                       const char* filename)
{
    if (gtraces.empty()) {
        fprintf(stdout, "iogdiagnose: No GTrace data\n");
        return;
    }

    FILE* fp = fopen(filename, "w");
    if (!static_cast<bool>(fp)) {
        fprintf(stdout,
                "iogdiagnose: Failed to open %s for write access\n", filename);
        return;
    }

    // Marks beginning of a multi
    const GTraceEntry magic(gtraces.size(), GTRACE_REVISION);

    fwrite(&magic, sizeof(magic), 1, fp);  // Indicates v2 gtrace binary
    for (const GTraceBuffer& buf : gtraces)
        // TODO(gvdl): how about some error checking?
        fwrite(buf.data(), sizeof(*buf.data()), buf.size(), fp);
    fclose(fp);
}

// Simple helpers for report dumper
inline int bitIsSet(const uint32_t value, const uint32_t bit)
    { return static_cast<bool>(value & bit); }
inline int isStateSet(const IOGReport& fbState, uint32_t bit)
    { return bitIsSet(fbState.stateBits, bit); }
inline mach_timebase_info_data_t getMachTimebaseInfo()
{
    mach_timebase_info_data_t ret = { 0 };
    mach_timebase_info(&ret);
    return ret;
}
inline tm getTime()
{
    tm ret;
    time_t rawtime = 0; time(&rawtime);
    localtime_r(&rawtime, &ret);
    return ret;
}

void dumpGTraceReport(const IOGDiagnose& diag,
                      const vector<GTraceBuffer>& gtraces,
                      const bool bDumpToFile)
{
    if (diag.version < 7) {
        fprintf(stderr, "iogdiagnose: Can't read pre v7 IOGDiagnose reports\n");
        return;
    }

    const mach_timebase_info_data_t info = getMachTimebaseInfo();
    const tm timeinfo = getTime();
    char filename[kFILENAME_LENGTH] = {0};
    strftime(filename, sizeof(filename),
             "/tmp/com.apple.iokit.IOGraphics_%F_%H-%M-%S.txt", &timeinfo);

    FILE* const fp = (bDumpToFile) ? fopen(filename, "w+") : nullptr;
    FILE* const outfile = (static_cast<bool>(fp)) ? fp : stdout;
    foutsep(outfile, "IOGRAPHICS REPORT");
    strftime(filename, sizeof(filename), "Report date: %F %H:%M:%S", &timeinfo);
    fprintf(outfile, "%s\n", filename);

    const auto fbCount = std::min<uint64_t>(
        diag.framebufferCount, IOGRAPHICS_MAXIMUM_FBS);

    fprintf(outfile, "Report version: %#llx\n", diag.version);
    fprintf(outfile, "Boot Epoch Time: %llu\n", diag.systemBootEpochTime);
    fprintf(outfile, "Number of framebuffers: %llu%s\n\n",
        diag.framebufferCount,
        ((fbCount != diag.framebufferCount)
            ? " (warning: some fbs not reported)" : ""));

    for (uint32_t i = 0; i < fbCount; i++) {
        const IOGReport& fbState = diag.fbState[i];

        fprintf(outfile, "\t%s::",
                (*fbState.objectName) ? fbState.objectName : "UNKNOWN::");
        fputs((*fbState.framebufferName) ? fbState.framebufferName : "IOFB?",
                outfile);
        fprintf(outfile,
                " (%u %#llx)\n", fbState.dependentIndex, fbState.regID);
        if (fbState.aliasID)
            fprintf(outfile, "\t\tAlias     : 0x%08x\n", fbState.aliasID);

        fprintf(outfile, "\t\tI-State   : 0x%08x (b", fbState.stateBits);
        for (uint32_t mask = 0x80000000; 0 != mask; mask >>= 1)
            fprintf(outfile, "%d", isStateSet(fbState, mask));
        fprintf(outfile, ")\n");

        fprintf(outfile, "\t\tStates    : OP:ON:US:PS:CS:CC:CL:CO:DS:MR:SG:WL:NA:SN:SS:SA:PP:SP:MS:MP:MU:PPS: NOTIFIED :   WSAA   \n");
        fprintf(outfile, "\t\tState Bits: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:0x%01x:0x%08x:0x%08x\n",
                isStateSet(fbState, kIOGReportState_Opened),
                isStateSet(fbState, kIOGReportState_Online),
                isStateSet(fbState, kIOGReportState_Usable),
                isStateSet(fbState, kIOGReportState_Paging),
                isStateSet(fbState, kIOGReportState_Clamshell),
                isStateSet(fbState, kIOGReportState_ClamshellCurrent),
                isStateSet(fbState, kIOGReportState_ClamshellLast),
                isStateSet(fbState, kIOGReportState_ClamshellOffline),
                isStateSet(fbState, kIOGReportState_SystemDark),
                isStateSet(fbState, kIOGReportState_Mirrored),
                isStateSet(fbState, kIOGReportState_SystemGated),
                isStateSet(fbState, kIOGReportState_WorkloopGated),
                isStateSet(fbState, kIOGReportState_NotificationActive),
                isStateSet(fbState, kIOGReportState_ServerNotified),
                isStateSet(fbState, kIOGReportState_ServerState),
                isStateSet(fbState, kIOGReportState_ServerPendingAck),
                isStateSet(fbState, kIOGReportState_PowerPendingChange),
                isStateSet(fbState, kIOGReportState_SystemPowerAckTo),
                isStateSet(fbState, kIOGReportState_IsMuxSwitching),
                isStateSet(fbState, kIOGReportState_PowerPendingMuxChange),
                isStateSet(fbState, kIOGReportState_Muted),
                fbState.pendingPowerState,
                fbState.notificationGroup,
                fbState.wsaaState);

        if ((diag.version >= 9) && (kIOReturnSuccess != fbState.lastWSAAStatus)) {
            fprintf(outfile,
                "\t\t**** WARNING: setAttribute(WSAA) failed: %#x\n",
                fbState.lastWSAAStatus);
        }

        fprintf(outfile, "\t\tA-State   : 0x%08x (b", fbState.externalAPIState);
        for (uint32_t mask = 0x80000000; 0 != mask; mask >>= 1)
            fprintf(outfile, "%d", bitIsSet(fbState.externalAPIState, mask));
        fprintf(outfile, ")\n");

        fprintf(outfile, "\t\tMode ID   : %#x\n", fbState.lastSuccessfulMode);
        fprintf(outfile, "\t\tSystem    : %llu (%#llx) (%u)\n",
                fbState.systemOwner, fbState.systemOwner,
                fbState.systemGatedCount);
        fprintf(outfile, "\t\tController: %llu (%#llx) (%u)\n",
                fbState.workloopOwner, fbState.workloopOwner,
                fbState.workloopGatedCount);


        for (int gi = 0; gi < IOGRAPHICS_MAXIMUM_REPORTS; ++gi) {
            const auto& group = fbState.notifications[gi];
            if (!group.groupID)
                continue;

            fprintf(outfile, "\n\t\tGroup ID: %#llx\n", group.groupID - 1);
            for (int si = 0; si < IOGRAPHICS_MAXIMUM_REPORTS; ++si) {
                const auto& stamp = group.stamp[si];
                if (stamp.lastEvent || stamp.start || stamp.end || *stamp.name)
                {
                    fprintf(outfile, "\t\t\tComponent   : %s\n", stamp.name);
                    fprintf(outfile, "\t\t\tLast Event  : %u (%#x)\n",
                            stamp.lastEvent, stamp.lastEvent);
                    fprintf(outfile, "\t\t\tStamp Start : %#llx\n", stamp.start);
                    fprintf(outfile, "\t\t\tStamp End   : %#llx\n", stamp.end);
                    fprintf(outfile, "\t\t\tStamp Delta : ");
                    if (stamp.start <= stamp.end) {
                        fprintf(outfile, "%llu ns\n",
                                ((stamp.end - stamp.start)
                                     * static_cast<uint64_t>(info.numer))
                                    / static_cast<uint64_t>(info.denom));
                    } else
                        fprintf(outfile, "Notifier Active\n");
                }
            }
        }
    }

    // Tokenized logging data
    dumpTokenBuffer(outfile, gtraces);
    fflush(outfile);

    foutsep(outfile);
    agdcdiagnose(outfile);
    fflush(outfile);

    if (static_cast<bool>(fp))
        fclose(fp);
}
}; // namespace

int main(int argc, char * argv[])
{
    bool                 bDumpToFile = false;
    bool                 bBinaryToFile = false;
    int                  flagInd = 0;
    int                  flag = 0;
    char                 inputFilename[256] = { 0 };
    static struct option opts[] = {
        "file",   optional_argument,  nullptr, 'f',
        "binary", required_argument,  nullptr, 'b',
        nullptr,  no_argument,        nullptr,  0 ,
    };
    const char*          argKeys = "fb:";

    while (-1 != (flag = getopt_long(argc, argv, argKeys, opts, &flagInd)) ) {
        switch (flag) {
        case 'f':
            bDumpToFile = true;
            break;
        case 'b':
            if (optarg != nullptr) {
                const size_t len = strlen(optarg);
                if (len <= sizeof(inputFilename)) {
                    strncpy(inputFilename, optarg, len);
                    bBinaryToFile = true;
                }
            }
            break;
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int exitValue = EXIT_SUCCESS;
    char *error = nullptr;
    IOConnect dc; // Diagnostic connection
    kern_return_t err = openDiagnostics(&dc, &error);
    if (err) {
        fprintf(stderr, "%s %s (%#x)\n", error, mach_error_string(err), err);
        exit(EXIT_FAILURE);
    }

    vector<GTraceBuffer> gtraces;
    if (bBinaryToFile) {
        err = fetchGTraceBuffers(dc, &gtraces);
        if (err)
            exitValue = EXIT_FAILURE; // error reported by library
        else
            writeGTraceBinary(gtraces, inputFilename);
        exit(exitValue);
    }

    IOGDiagnose report = { 0 };
    err = iogDiagnose(dc, &report, sizeof(report), &gtraces, &error);
    if (err) {
        fprintf(stderr, "%s %s (%#x)\n", error, mach_error_string(err), err);
        exitValue = EXIT_FAILURE;
    }
    else
        dumpGTraceReport(report, gtraces, bDumpToFile);

    return exitValue;
}

