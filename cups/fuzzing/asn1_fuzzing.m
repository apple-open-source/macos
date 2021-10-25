#include <Foundation/Foundation.h>

#include "config.h"
#include "array.c"
#include "ipp-support.c"
#include "options.c"
#include "transcode.c"
#include "usersys.c"
#include "language.c"
#include "thread.c"
#include "ipp.c"
#include "globals.c"
#include "debug.c"
#include "file.c"
#include "dir.c"
#include "snmp.c"

#include "stubs.m"

int _asn1_fuzzing(Boolean verbose, const uint8_t* data, size_t len)
{
    int save = gVerbose;
    gVerbose = verbose;

    cups_snmp_t packet;
    bzero(&packet, sizeof(packet));

    uint8_t* localData = (uint8_t*) malloc(len);
    memmove(localData, data, len);
    asn1_decode_snmp(localData, len, &packet);
    Boolean matches = memcmp(localData, data, len) == 0;
    free(localData);

    gVerbose = save;

    if (! matches) {
        NSLog(@"asn1_decode_snmp mutated input buffer!");
        return 1;
    }

    return 0;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *buffer, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *buffer, size_t size)
{
    return _asn1_fuzzing(false, buffer, size);
}
