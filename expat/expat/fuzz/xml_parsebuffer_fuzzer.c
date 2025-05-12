/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "expat.h"
#include "siphash.h"

// Macros to convert preprocessor macros to string literals. See
// https://gcc.gnu.org/onlinedocs/gcc-3.4.3/cpp/Stringification.html
#define xstr(s) str(s)
#define str(s) #s

#ifndef __APPLE__
// The encoder type that we wish to fuzz should come from the compile-time
// definition `ENCODING_FOR_FUZZING`. This allows us to have a separate fuzzer
// binary for
#ifndef ENCODING_FOR_FUZZING
#  error "ENCODING_FOR_FUZZING was not provided to this fuzz target."
#endif
#endif /* __APPLE__ */

// 16-byte deterministic hash key.
static unsigned char hash_key[16] = "FUZZING IS FUN!";

static void XMLCALL
start(void *userData, const XML_Char *name, const XML_Char **atts) {
  (void)userData;
#ifdef __APPLE__
  (void)strlen(name);
  for (unsigned i = 0; atts[i] != NULL; i++)
      (void)strlen(atts[i]);
#else
  (void)name;
  (void)atts;
#endif
}
static void XMLCALL
end(void *userData, const XML_Char *name) {
  (void)userData;
#ifdef __APPLE__
  (void)strlen(name);
#else
  (void)name;
#endif
}

#ifdef __APPLE__
static const char *encodings[] = {
  "US-ASCII",
  "ISO-8859-1",
  "UTF-8",
  "UTF-16",
  "UTF-16BE",
  "UTF-16LE",
  "UNKNOWN_ENCODING",
};
const unsigned encodingsCount = sizeof(encodings) / sizeof(const char*);
#endif /* __APPLE__ */

static void XMLCALL
may_stop_character_handler(void *userData, const XML_Char *s, int len) {
  XML_Parser parser = (XML_Parser)userData;
  if (len > 1 && s[0] == 's') {
    XML_StopParser(parser, s[1] == 'r' ? XML_FALSE : XML_TRUE);
  }
}

static void
ParseOneInput(XML_Parser p, const uint8_t *data, size_t size) {
  // Set the hash salt using siphash to generate a deterministic hash.
  struct sipkey *key = sip_keyof(hash_key);
  XML_SetHashSalt(p, (unsigned long)siphash24(data, size, key));
  (void)sip24_valid;

  XML_SetUserData(p, p);
  XML_SetElementHandler(p, start, end);
  XML_SetCharacterDataHandler(p, may_stop_character_handler);
  void *buf = XML_GetBuffer(p, size);
  assert(buf);
  memcpy(buf, data, size);
  XML_ParseBuffer(p, size, 0);
  buf = XML_GetBuffer(p, size);
  if (buf == NULL) {
    return;
  }
  memcpy(buf, data, size);
  if (XML_ParseBuffer(p, size, 1) == XML_STATUS_ERROR) {
    XML_ErrorString(XML_GetErrorCode(p));
  }
  XML_GetCurrentLineNumber(p);
  if (size % 2) {
    XML_ParserReset(p, NULL);
  }
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0)
    return 0;

#ifdef __APPLE__
  XML_Parser parentParser = XML_ParserCreate(encodings[size % encodingsCount]);
#else
  XML_Parser parentParser = XML_ParserCreate(xstr(ENCODING_FOR_FUZZING));
#endif // __APPLE__

  assert(parentParser);
  ParseOneInput(parentParser, data, size);
  // not freed yet, but used later and freed then

  XML_Parser namespaceParser = XML_ParserCreateNS(NULL, '!');
  assert(namespaceParser);
  ParseOneInput(namespaceParser, data, size);
  XML_ParserFree(namespaceParser);

  XML_Parser externalEntityParser
      = XML_ExternalEntityParserCreate(parentParser, "e1", NULL);
  assert(externalEntityParser);
  ParseOneInput(externalEntityParser, data, size);
  XML_ParserFree(externalEntityParser);

  XML_Parser externalDtdParser
      = XML_ExternalEntityParserCreate(parentParser, NULL, NULL);
  assert(externalDtdParser);
  ParseOneInput(externalDtdParser, data, size);
  XML_ParserFree(externalDtdParser);

  // finally frees this parser which served as parent
  XML_ParserFree(parentParser);
  return 0;
}
