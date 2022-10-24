/*
 * xpath.h: Header for fuzz targets
 *
 * See Copyright for the status of this software.
 */

#ifndef __XML_XSLT_TESTS_FUZZ_H__
#define __XML_XSLT_TESTS_FUZZ_H__

#include <stddef.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

__attribute__((visibility("default"))) int
LLVMFuzzerInitialize(int *argc, char ***argv);

__attribute__((visibility("default"))) int
LLVMFuzzerTestOneInput(const char *data, size_t size);

int
xsltFuzzXPathInit(int *argc_p, char ***argv_p, const char *dir);

xmlXPathObjectPtr
xsltFuzzXPath(const char *data, size_t size);

void
xsltFuzzXPathFreeObject(xmlXPathObjectPtr obj);

void
xsltFuzzXPathCleanup(void);

int
xsltFuzzXsltInit(int *argc_p, char ***argv_p, const char *dir);

xmlChar *
xsltFuzzXslt(const char *data, size_t size);

void
xsltFuzzXsltCleanup(void);

#endif
