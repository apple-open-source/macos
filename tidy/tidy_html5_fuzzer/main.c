/*
 *  main.c
 *  tidy_fuzzer_html5
 *
 *  Created by fabio2 on 2/4/21.
 *  Copyright © 2021 Apple Inc. All rights reserved.
 */

#include <limits.h>
#include "buffio.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
void run_tidy_parser(TidyBuffer* data_buffer, TidyBuffer* output_buffer, TidyBuffer* error_buffer);

void run_tidy_parser(TidyBuffer* data_buffer, TidyBuffer* output_buffer, TidyBuffer* error_buffer)
{
    TidyDoc tdoc = tidyCreate();
    if (tidySetErrorBuffer(tdoc, error_buffer) < 0) {
        abort();
    }
    // TODO FAB: we should iterate on all options HTML5, ...
    tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
    tidyOptSetBool(tdoc, TidyForceOutput, yes);

    if (tidyParseBuffer(tdoc, data_buffer) >= 0 && tidyCleanAndRepair(tdoc) >= 0 && tidyRunDiagnostics(tdoc) >= 0) {
        tidySaveBuffer(tdoc, output_buffer);
    }
    tidyRelease(tdoc);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    TidyBuffer data_buffer;
    TidyBuffer output_buffer;
    TidyBuffer error_buffer;
    tidyBufInit(&data_buffer);
    tidyBufInit(&output_buffer);
    tidyBufInit(&error_buffer);

    if (size > UINT_MAX)
        return 0;

    tidyBufAttach(&data_buffer, (byte*)data, (unsigned)size);
    run_tidy_parser(&data_buffer, &output_buffer, &error_buffer);

    tidyBufFree(&error_buffer);
    tidyBufFree(&output_buffer);
    tidyBufDetach(&data_buffer);
    return 0;
}
