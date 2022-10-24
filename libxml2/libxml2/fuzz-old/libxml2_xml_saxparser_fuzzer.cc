// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright 2017 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <functional>
#include <limits>
#include <string>

#include "libxml/parser.h"
#include "libxml/parserInternals.h"
#include "libxml/xmlsave.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void errorCallback(void* ctx, const char* msg, ...) {
  xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
  xmlErrorPtr error = xmlCtxtGetLastError(ctxt);
  if (error != NULL && error->level == XML_ERR_FATAL)
    xmlStopParser(ctxt);
}

static xmlSAXHandler xmlSAXHandlerStruct = {
  NULL, /* internalSubset */
  NULL, /* isStandalone */
  NULL, /* hasInternalSubset */
  NULL, /* hasExternalSubset */
  NULL, /* resolveEntity */
  NULL, /* getEntity */
  NULL, /* entityDecl */
  NULL, /* notationDecl */
  NULL, /* attributeDecl */
  NULL, /* elementDecl */
  NULL, /* unparsedEntityDecl */
  NULL, /* setDocumentLocator */
  NULL, /* startDocument */
  NULL, /* endDocument */
  NULL, /* startElement */
  NULL, /* endElement */
  NULL, /* reference */
  NULL, /* characters */
  NULL, /* ignorableWhitespace */
  NULL, /* processingInstruction */
  NULL, /* comment */
  NULL, /* xmlParserWarning */
  errorCallback, /* xmlParserError */
  NULL, /* xmlParserError */
  NULL, /* getParameterEntity */
  NULL, /* cdataBlock; */
  NULL, /* externalSubset; */
  XML_SAX2_MAGIC,
  NULL,
  NULL, /* startElementNs */
  NULL, /* endElementNs */
  NULL  /* xmlStructuredErrorFunc */
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Test default empty options value and some random combination.
  std::string data_string(reinterpret_cast<const char*>(data), size);
  const std::size_t data_hash = std::hash<std::string>()(data_string);
  const int max_option_value = std::numeric_limits<int>::max();
  const int random_option_value = data_hash % max_option_value;
  const int options[] = {0, random_option_value};

  for (const auto option_value : options) {
    // See testSAX() in xmllint.c.
    if (xmlParserInputBufferPtr inputBuffer = xmlParserInputBufferCreateMem((const char*)data, (int)size, XML_CHAR_ENCODING_NONE)) {
      if (xmlParserCtxtPtr ctxt = xmlNewParserCtxt()) {
        xmlCtxtUseOptions(ctxt, option_value | XML_PARSE_NOENT | XML_PARSE_NONET);
        xmlSAXHandlerPtr handler = &xmlSAXHandlerStruct;
        //XMLSAXHandlerSaver saver(ctxt, handler);
        xmlSAXHandlerPtr old_sax = ctxt->sax;
        ctxt->sax = handler;
        ctxt->userData = ctxt;
        if (xmlParserInputPtr inputStream = xmlNewIOInputStream(ctxt, inputBuffer, XML_CHAR_ENCODING_NONE)) {
          inputPush(ctxt, inputStream);
          xmlParseDocument(ctxt);
          if (xmlDocPtr doc = ctxt->myDoc) {
            ctxt->myDoc = nullptr;

            auto buf = xmlBufferCreate();
            assert(buf);
            auto ctxtSave = xmlSaveToBuffer(buf, NULL, 0);
            xmlSaveDoc(ctxtSave, doc);
            xmlSaveClose(ctxtSave);

            xmlFreeDoc(doc);
          }
        }
        ctxt->sax = old_sax;
        xmlFreeParserCtxt(ctxt);
      }
    }
  }

  return 0;
}
