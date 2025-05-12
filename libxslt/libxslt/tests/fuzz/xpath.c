/*
 * xpath.c: libFuzzer target for XPath expressions
 *
 * See Copyright for the status of this software.
 */

#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc_p, char ***argv_p) {
    const char *dir = getenv("STATIC_XML_FILE_DIR");
    if (dir && dir[0] == '\0')
        dir = NULL;
    return xsltFuzzXPathInit(argc_p, argv_p, dir);
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlXPathObjectPtr xpathObj = xsltFuzzXPath(data, size);
    xsltFuzzXPathFreeObject(xpathObj);

    return 0;
}
