//
//  Security_Fuzzing.c
//  Security
//

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "SecCertificateFuzzer.h"
#include "SecKeychainFuzzer.h"

#define SEC_FUZZER_MODE_ENV_VAR "SEC_FUZZER_MODE"

#define MODESTR_SECCERTIFICATE "SecCertificate"
#define MODESTR_SECKEYCHAIN    "SecKeychain"

int LLVMFuzzerInitialize(int *argc, char ***argv);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len);

typedef enum {
    MODE_UNKNOWN = 0,
    MODE_SECCERTIFICATE,
    MODE_SECKEYCHAIN
} mode_enum;

static mode_enum mode = MODE_UNKNOWN;

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    char* mode_env = getenv(SEC_FUZZER_MODE_ENV_VAR);

    if (!mode_env || 0 == strncmp(mode_env, MODESTR_SECCERTIFICATE, strlen(MODESTR_SECCERTIFICATE))) {
        mode = MODE_SECCERTIFICATE;
    } else if (0 == strncmp(mode_env, MODESTR_SECKEYCHAIN, strlen(MODESTR_SECKEYCHAIN))) {
        mode = MODE_SECKEYCHAIN;
    }

    if (mode == MODE_UNKNOWN) {
        printf("Unknown mode (from env var %s): %s\n", SEC_FUZZER_MODE_ENV_VAR, mode_env);
        exit(1);
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len)
{
    if (mode == MODE_SECCERTIFICATE) {
        SecCertificateFuzzer(data, len);
    } else if (mode == MODE_SECCERTIFICATE) {
        SecKeychainFuzzer(data, len);
    }

    return 0;
}
