/*
 * xpath.c: a libFuzzer target to test XPath and XPointer expressions.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/parser.h>
#include <libxml/xpointer.h>
#include "fuzz.h"

extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t maxSize);
extern size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxSize, unsigned int seed);

size_t
LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxSize, unsigned int seed) {
    xmlFuzzRndSetSeed(seed);

    const size_t optionsSize = sizeof(int);
    if (size < optionsSize)
        return LLVMFuzzerMutate(data, size, maxSize);

    // Mutate libxml2 parsing options in first byte of input (10% chance).
    if (xmlFuzzRnd() % 10 == 1)
        *((int *)&data[0]) = (int)xmlFuzzRnd();

    return optionsSize + LLVMFuzzerMutate(data + optionsSize, size - optionsSize, maxSize - optionsSize);
}

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlDocPtr doc;
    const char *expr, *xml;
    size_t exprSize, xmlSize;
    xmlCharEncoding encoding;
    int opts;

    if (size > 10000)
        return(0);

    xmlFuzzDataInit(data, size);
    encoding = (xmlCharEncoding)(xmlFuzzDataHash() % 23); /* See <libxml/encoding.h>. */
    opts = xmlFuzzReadInt() | XML_PARSE_NONET;

    expr = xmlFuzzReadString(&exprSize);
    xml = xmlFuzzReadString(&xmlSize);

    /* Recovery mode allows more input to be fuzzed. */
    doc = xmlReadMemory(xml, xmlSize, NULL, xmlGetCharEncodingName(encoding), opts | XML_PARSE_RECOVER);
    if (doc != NULL) {
        xmlXPathContextPtr xpctxt = xmlXPathNewContext(doc);

        /* Operation limit to avoid timeout */
        xpctxt->opLimit = 500000;

        xmlXPathFreeObject(xmlXPtrEval(BAD_CAST expr, xpctxt));
        xmlXPathFreeContext(xpctxt);
    }
    xmlFreeDoc(doc);

    xmlFuzzDataCleanup();
    xmlResetLastError();

    return(0);
}

