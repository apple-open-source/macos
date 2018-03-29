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
 MX: Pending Mux Change (bool)
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

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <memory.h>
#include <time.h>
#include <spawn.h>
#include <sys/wait.h>


#include "IOGDiagnoseUtils/IOGDiagnoseUtils.h"
#include "../GTrace/GTraceTypes.h"


#define kFILENAME_LENGTH                64


void print_usage( const char * name );
void agdcdiagnose(FILE * fOut);


void print_usage( const char * name )
{
    fprintf(stdout, "%s options:\n", name);

    fprintf(stdout, "\t--file | -f\n");
    fprintf(stdout, "\t\tWrite report to file /tmp/com.apple.iokit.IOGraphics_YYYY_MM__DD_HH-MM-SS.txt (instead of stdout)\n");

    fprintf(stdout, "\n\t--binary | -b binary_file\n");

    fprintf(stdout, "\n");
    fflush(stdout);
}

extern char **environ;

void agdcdiagnose(FILE * fOut)
{
    pid_t       pid = 0;
    int         status = -1;
    char        * const agdcArgs[] = {const_cast<char *>("/System/Library/Extensions/AppleGraphicsControl.kext/Contents/MacOS/AGDCDiagnose"),
        const_cast<char *>("-a"),
        nullptr};
    if (NULL != fOut) {
        fprintf(fOut, "\n\n------------------------------------- AGDC REPORT -------------------------------------\n\n");
        status = posix_spawn(&pid, agdcArgs[0], NULL, NULL, agdcArgs, environ);
        if (0 == status) {
            (void)waitpid(pid, &status, 0);
        } else {
            fprintf(fOut, "\tAGDCDiagnose failed to launch\n");
        }
        fprintf(fOut, "\n---------------------------------------------------------------------------------------\n");
    }
}

uint32_t sizeToLines( uint32_t bufSize )
{
    uint32_t    lines = 0;
    if (bufSize > (kGTraceMaximumLineCount * sizeof(sGTrace))) {
        lines = kGTraceMaximumLineCount;
    }
    else if (bufSize >= sizeof(sGTrace)) {
        lines = (1 << (32 - (__builtin_clz(bufSize) + 1))) / sizeof(sGTrace);
    }
    return (lines);
}

bool dumpTokenBuffer(FILE * fOut, uint32_t lastToken, const uint32_t tokenLineCount, const uint32_t tokenSize, uint64_t * tokenBuffer)
{
    sGTrace     * traceBuffer = reinterpret_cast<sGTrace *>(tokenBuffer);
    uint32_t    traceLines = 0;
    uint32_t    currentLine = 0;
    bool        bRet = false;

    fprintf(fOut, "\n\t\tLast Token Recorded: ");
    if (NULL != tokenBuffer) {
        // Buffer size needs to be at least enough for a single line of token data.
        if (tokenSize >= sizeof(sGTrace)) {
            traceLines = sizeToLines(tokenSize);
            if (0 != traceLines) {
                bRet = true;

                // Last token points to next free entry.
                lastToken--;

                fprintf(fOut, "%u\n", lastToken);
                fprintf(fOut, "\t\tToken Buffer Lines : %u\n", tokenLineCount);
                fprintf(fOut, "\t\tToken Buffer Size  : %u\n", tokenSize);
                fprintf(fOut, "\t\tToken Buffer Data  :\n");

                // In case there is a buffer size discrepency, make sure lastToken is within the range of the traceLines
                if (lastToken > traceLines) {
                    lastToken = traceLines;
                }

                for (currentLine = 0; currentLine < lastToken; currentLine++) {
                    fprintf(fOut, "\t\tTkn: %04u\tTS: %llu\tLn: %u\tC: %#llx\tCTID: %u-%#llx\tOID: %#llx\tTag: %#llx\tA: %#llx-%#llx-%#llx-%#llx\n",
                            currentLine,
                            traceBuffer[currentLine].traceEntry.timestamp,
                            traceBuffer[currentLine].traceEntry.traceID.ID.line,
                            traceBuffer[currentLine].traceEntry.traceID.ID.component,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.cpu,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.threadID,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.registryID,
                            traceBuffer[currentLine].traceEntry.argsTag.TAG.u64,
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[0],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[1],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[2],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[3]
                            );
                }
                // Mark active line
                fprintf(fOut, "\tTkn: %04u\tTS: %llu\tLn: %u\tC: %#llx\tCTID: %u-%#llx\tOID: %#llx\tTag: %#llx\tA: %#llx-%#llx-%#llx-%#llx\t<--\n",
                        currentLine,
                        traceBuffer[currentLine].traceEntry.timestamp,
                        traceBuffer[currentLine].traceEntry.traceID.ID.line,
                        traceBuffer[currentLine].traceEntry.traceID.ID.component,
                        traceBuffer[currentLine].traceEntry.threadInfo.TI.cpu,
                        traceBuffer[currentLine].traceEntry.threadInfo.TI.threadID,
                        traceBuffer[currentLine].traceEntry.threadInfo.TI.registryID,
                        traceBuffer[currentLine].traceEntry.argsTag.TAG.u64,
                        traceBuffer[currentLine].traceEntry.args.ARGS.u64s[0],
                        traceBuffer[currentLine].traceEntry.args.ARGS.u64s[1],
                        traceBuffer[currentLine].traceEntry.args.ARGS.u64s[2],
                        traceBuffer[currentLine].traceEntry.args.ARGS.u64s[3]
                        );
                currentLine++;
                for (; currentLine < traceLines; currentLine++) {
                    fprintf(fOut, "\t\tTkn: %04u\tTS: %llu\tLn: %u\tC: %#llx\tCTID: %u-%#llx\tOID: %#llx\tTag: %#llx\tA: %#llx-%#llx-%#llx-%#llx\n",
                            currentLine,
                            traceBuffer[currentLine].traceEntry.timestamp,
                            traceBuffer[currentLine].traceEntry.traceID.ID.line,
                            traceBuffer[currentLine].traceEntry.traceID.ID.component,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.cpu,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.threadID,
                            traceBuffer[currentLine].traceEntry.threadInfo.TI.registryID,
                            traceBuffer[currentLine].traceEntry.argsTag.TAG.u64,
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[0],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[1],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[2],
                            traceBuffer[currentLine].traceEntry.args.ARGS.u64s[3]
                            );
                }
            } else {
                fprintf(fOut, "-3 (Token Data Count Invalid (%d vs %d))\n", traceLines, tokenSize);
            }
        } else {
            fprintf(fOut, "-2 (No Token Data Recorded)\n");
        }
    } else {
        fprintf(fOut, "-1 (No Token Data Recorded)\n");
    }
    fprintf(fOut, "\n");

    return (bRet);
}

void writeGTraceBinary( const IOGDiagnose * report, const char * filename )
{
    FILE *  fp = NULL;

    fp = fopen(filename, "wb");
    if (fp) {
        // Tokenized logging data (version 3+)
        if ((report->version >= 3)
            && report->tokenSize)
        {
            fwrite(report->tokenBuffer, report->tokenSize, 1, fp);
            fflush(fp);
            fclose(fp);
        } else {
            fprintf(stdout, "iogdiagnose: No GTrace data\n");
        }
    } else {
        fprintf(stdout, "iogdiagnose: Failed to open %s for write access\n",
                filename);
    }
}

void dumpGTraceReport( const IOGDiagnose * report, const bool bDumpToFile )
{
    FILE *                      fOut = NULL;
    FILE *                      fp = NULL;
    tm *                        timeinfo = NULL;
    const IOGReport *           fbState = NULL;
    time_t                      rawtime = 0;
    uint32_t                    index = 0;
    uint32_t                    mask = 0x80000000;
    unsigned int                groupIndex = 0;
    unsigned int                stampIndex = 0;
    mach_timebase_info_data_t   info = {0};
    char                        filename[kFILENAME_LENGTH] = {0};

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(filename, kFILENAME_LENGTH, "/tmp/com.apple.iokit.IOGraphics_%F_%H-%M-%S.txt",
             timeinfo);
    if (bDumpToFile) {
        fp = fopen(filename, "w+");
    }
    if (NULL == fp) {
        fOut= stdout;
    } else {
        fOut = fp;
    }
    fprintf(fOut, "---------------------------------- IOGRAPHICS REPORT ----------------------------------\n\n");
    strftime(filename, kFILENAME_LENGTH, "Report date: %F %H:%M:%S\n", timeinfo);
    fprintf(fOut, "%s", filename);
    if (report) {
        if (report->version <= 5) {
            fprintf(fOut, "Report version: %#llx\n", report->version);
            fprintf(fOut, "Boot Epoch Time: %llu\n", report->systemBootEpochTime);
            fprintf(fOut, "Number of framebuffers: %llu\n\n",
                    report->framebufferCount);
            while(index < report->framebufferCount) {
                fbState = &(report->fbState[index]);
                index++;

                if (strlen(fbState->objectName)) {
                    fprintf(fOut, "\t%s::", fbState->objectName);
                } else {
                    fprintf(fOut, "\tUNKNOWN::");
                }

                if (strlen(fbState->framebufferName)) {
                    fprintf(fOut, "%s", fbState->framebufferName);
                } else {
                    fprintf(fOut, "IOFB?");
                }
                fprintf(fOut, " (%u %#llx)\n",
                        fbState->dependentIndex, fbState->regID);

                fprintf(fOut, "\t\tI-State   : 0x%08x (b", fbState->stateBits);
                for (mask = 0x80000000; 0 != mask; mask >>= 1) {
                    fprintf(fOut, "%d", (fbState->stateBits & mask) ? 1 : 0);
                }
                fprintf(fOut, ")\n");

                if (report->version >= 4) {
                    fprintf(fOut, "\t\tStates    : OP:US:PS:CS:CC:CL:DS:MR:SG:WL:NA:SN:SS:SA:PP:SP:MX:PPS: NOTIFIED :   WSAA   \n");
                    fprintf(fOut, "\t\tState Bits: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:0x%01x:0x%08x:0x%08x\n",
                            fbState->stateBits & kIOGReportState_Opened ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Usable ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Paging ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Clamshell ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ClamshellCurrent ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ClamshellLast ? 1 : 0,
                            fbState->stateBits & kIOGReportState_SystemDark ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Mirrored ? 1 : 0,
                            fbState->stateBits & kIOGReportState_SystemGated ? 1 : 0,
                            fbState->stateBits & kIOGReportState_WorkloopGated ? 1 : 0,
                            fbState->stateBits & kIOGReportState_NotificationActive ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerNotified ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerState ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerPendingAck ? 1 : 0,
                            fbState->stateBits & kIOGReportState_PowerPendingChange ? 1 : 0,
                            fbState->stateBits & kIOGReportState_SystemPowerAckTo ? 1 : 0,
                            fbState->stateBits & kIOGReportState_PowerPendingMuxChange ? 1 : 0,
                            fbState->pendingPowerState,
                            fbState->notificationGroup,
                            fbState->wsaaState );
                } else {
                    fprintf(fOut, "\t\tStates    : OP:US:PS:CS:CC:CL:DS:MR:SG:WL:NA:SN:SS:SA:PP:MX:PPS: NOTIFIED :   WSAA   \n");
                    fprintf(fOut, "\t\tState Bits: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:0x%01x:0x%08x:0x%08x\n",
                            fbState->stateBits & kIOGReportState_Opened ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Usable ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Paging ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Clamshell ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ClamshellCurrent ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ClamshellLast ? 1 : 0,
                            fbState->stateBits & kIOGReportState_SystemDark ? 1 : 0,
                            fbState->stateBits & kIOGReportState_Mirrored ? 1 : 0,
                            fbState->stateBits & kIOGReportState_SystemGated ? 1 : 0,
                            fbState->stateBits & kIOGReportState_WorkloopGated ? 1 : 0,
                            fbState->stateBits & kIOGReportState_NotificationActive ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerNotified ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerState ? 1 : 0,
                            fbState->stateBits & kIOGReportState_ServerPendingAck ? 1 : 0,
                            fbState->stateBits & kIOGReportState_PowerPendingChange ? 1 : 0,
                            fbState->stateBits & kIOGReportState_PowerPendingMuxChange ? 1 : 0,
                            fbState->pendingPowerState,
                            fbState->notificationGroup,
                            fbState->wsaaState );
                }

                fprintf(fOut, "\t\tA-State   : 0x%08x (b",
                        fbState->externalAPIState);
                for (mask = 0x80000000; 0 != mask; mask >>= 1) {
                    fprintf(fOut, "%d",
                            (fbState->externalAPIState & mask) ? 1 : 0);
                }
                fprintf(fOut, ")\n");
                if (report->version >= 2) {
                    fprintf(fOut, "\t\tMode ID   : %#x\n",
                            fbState->lastSuccessfulMode);
                }
                fprintf(fOut, "\t\tSystem    : %llu (%#llx) (%u)\n",
                        fbState->systemOwner,
                        fbState->systemOwner,
                        fbState->systemGatedCount);
                fprintf(fOut, "\t\tController: %llu (%#llx) (%u)\n",
                        fbState->workloopOwner,
                        fbState->workloopOwner,
                        fbState->workloopGatedCount);

                // Notification data (version 2+)
                if (report->version >= 2) {
                    mach_timebase_info(&info);

                    for (groupIndex = 0;
                         groupIndex < IOGRAPHICS_MAXIMUM_REPORTS;
                         groupIndex++)
                    {
                        if (0 != fbState->notifications[groupIndex].groupID) {
                            fprintf(fOut, "\n\t\tGroup ID: %#llx\n",
                                    fbState->notifications[groupIndex].groupID - 1);

                            for (stampIndex = 0;
                                 stampIndex < IOGRAPHICS_MAXIMUM_REPORTS;
                                 stampIndex++)
                            {
                                if ((0 != fbState->notifications[groupIndex].stamp[stampIndex].lastEvent) ||
                                    (0 != fbState->notifications[groupIndex].stamp[stampIndex].start) ||
                                    (0 != fbState->notifications[groupIndex].stamp[stampIndex].end) ||
                                    (0 != strlen(fbState->notifications[groupIndex].stamp[stampIndex].name)) )
                                {
                                    fprintf(fOut, "\t\t\tComponent   : %s\n",
                                            fbState->notifications[groupIndex].stamp[stampIndex].name);
                                    fprintf(fOut, "\t\t\tLast Event  : %u (%#x)\n",
                                            fbState->notifications[groupIndex].stamp[stampIndex].lastEvent,
                                            fbState->notifications[groupIndex].stamp[stampIndex].lastEvent);
                                    fprintf(fOut, "\t\t\tStamp Start : %#llx\n",
                                            fbState->notifications[groupIndex].stamp[stampIndex].start);
                                    fprintf(fOut, "\t\t\tStamp End   : %#llx\n",
                                            fbState->notifications[groupIndex].stamp[stampIndex].end);
                                    fprintf(fOut, "\t\t\tStamp Delta : ");
                                    if (fbState->notifications[groupIndex].stamp[stampIndex].start
                                        <= fbState->notifications[groupIndex].stamp[stampIndex].end)
                                    {
                                        fprintf(fOut, "%llu ns\n",
                                                ((fbState->notifications[groupIndex].stamp[stampIndex].end
                                                  - fbState->notifications[groupIndex].stamp[stampIndex].start)
                                                 * static_cast<uint64_t>(info.numer))
                                                / static_cast<uint64_t>(info.denom));
                                    } else {
                                        fprintf(fOut, "Notifier Active\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Tokenized logging data (version 3+)
            if (report->version >= 3)
            {
                (void)dumpTokenBuffer(fOut,
                                      report->tokenLine,
                                      report->tokenLineCount,
                                      report->tokenSize,
                                      const_cast<uint64_t *>(report->tokenBuffer));
            }
        } else {
            fprintf(fOut, "Unsupported report version: %#llx\n",
                    report->version);
        }
    } else {
        fprintf(fOut, "Parameter error: No reporting buffer.\n");
    }
    fprintf(fOut, "\n---------------------------------------------------------------------------------------\n");
    fflush(fOut);

    agdcdiagnose(fOut);
    fflush(fOut);
    if (NULL != fp) {
        fclose(fp);
    }
}

int main(int argc, char * argv[])
{
    IOGDiagnose                 * report = NULL;
    kern_return_t               kr = kIOReturnNoMemory;
    int                         exitValue = EXIT_SUCCESS;
    bool                        bDumpToFile = false;
    bool                        bBinaryToFile = false;
    int                         optsIndex = 0;
    int                         option = 0;
    char                        inputFilename[256]={0};
    static struct option        opts[] = {
        "file",     optional_argument,  NULL,       'f',
        "binary",   required_argument,  NULL,       'b',
        NULL,       no_argument,        NULL,       0,
    };
    const char *                argKeys = "fb:";

    if (argc > 1) {
        option = getopt_long(argc, argv, argKeys, opts, &optsIndex);
        if (-1 == option) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        do {
            switch (option) {
                case 'f': {
                    bDumpToFile = true;
                    break;
                }
                case 'b': {
                    if (optarg != NULL) {
                        const size_t len = strlen(optarg);
                        if (len <= sizeof(inputFilename)) {
                            strncpy(inputFilename, optarg, len);
                            bBinaryToFile = true;
                        }
                    }
                    break;
                }
                default: {
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
            }

            option = getopt_long(argc, argv, argKeys, opts, &optsIndex);
        } while (-1 != option);
    }

    size_t reportLength = sizeof(IOGDiagnose);
    report = (IOGDiagnose *)malloc(reportLength);
    if (NULL != report) {
        const char *error = NULL;
        kr = iogDiagnose(report,
                         reportLength,
                         IOGRAPHICS_DIAGNOSE_VERSION,
                         &error);
        if (kIOReturnSuccess != kr) {
            fprintf(stderr, "%s %s (%#x)\n",
                    error, mach_error_string(kr), kr);
        }
    } else {
        fprintf(stderr, "IOGDiagnose malloc failed %s (%#x)\n",
                mach_error_string(kr), kr);
    }

    if (kIOReturnSuccess == kr) {
        if (bBinaryToFile) {
            writeGTraceBinary( report, inputFilename );
        } else {
            dumpGTraceReport( report, bDumpToFile );
        }
    } else {
        exitValue = EXIT_FAILURE;
    }
    
    if (NULL != report) {
        free(report);
    }
    
    exit(exitValue);
}

