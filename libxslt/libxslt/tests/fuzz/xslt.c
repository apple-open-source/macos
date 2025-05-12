/*
 * xslt.c: libFuzzer target for XSLT stylesheets
 *
 * See Copyright for the status of this software.
 */

#include "fuzz.h"
#include <libxml/globals.h>

int
LLVMFuzzerInitialize(int *argc_p, char ***argv_p) {
    const char *dir = getenv("STATIC_XML_FILE_DIR");
    if (dir && dir[0] == '\0')
        dir = NULL;
    return xsltFuzzXsltInit(argc_p, argv_p, dir);
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlChar *result = xsltFuzzXslt(data, size);
    xmlFree(result);

    return 0;
}
