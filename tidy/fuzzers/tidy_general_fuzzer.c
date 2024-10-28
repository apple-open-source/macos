/*
 * Copyright 2021 Google LLC
 * Copyright 2024 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "buffio.h"
#include "tidy.h"

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxSize, unsigned int seed);
extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t maxSize);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

// All boolean options. These will be set randomly
// based on the fuzzer data.
TidyOptionId bool_options[] = {
  TidyHideEndTags,
  TidyJoinClasses, 
  TidyJoinStyles, 
  TidyKeepFileTimes, 
  /* TidyKeepTabs, */
  TidyLiteralAttribs, 
  TidyLogicalEmphasis, 
  TidyLowerLiterals, 
  TidyAsciiChars,
  TidyBodyOnly,
  TidyBreakBeforeBR,
  TidyDecorateInferredUL,
  TidyDropEmptyParas,
  TidyDropFontTags,
  TidyDropPropAttrs,
  TidyEmacs,
  TidyEncloseBlockText,
  TidyEncloseBodyText,
  TidyEscapeCdata,
  TidyFixBackslash,
  TidyFixComments,
  TidyFixUri, 
  TidyForceOutput, 
  /* TidyGDocClean, */
  TidyHideComments,
  TidyIndentAttributes,
  TidyIndentCdata,
  TidyMark, 
  TidyXmlTags, 
  TidyMakeClean,
  /* TidyAnchorAsName, */
  /* TidyMergeEmphasis, */
  TidyMakeBare, 
  /* TidyMetaCharset, */
  /* TidyMuteShow, */
#if SUPPORT_ASIAN_ENCODINGS
  TidyNCR, 
#endif
  TidyNumEntities, 
  /* TidyOmitOptionalTags, */
#if SUPPORT_ASIAN_ENCODINGS
  TidyPunctWrap, 
#endif
  TidyQuiet,
  TidyQuoteAmpersand,  
  TidyQuoteMarks, 
  TidyQuoteNbsp, 
  TidyReplaceColor, 
  /* TidyShowFilename, */
  /* TidyShowInfo, */
  TidyShowMarkup, 
  /* TidyShowMetaChange, */
  TidyShowWarnings, 
  /* TidySkipNested, */
  TidyUpperCaseAttrs,
  TidyUpperCaseTags, 
  TidyVertSpace,
  /* TidyWarnPropAttrs, */
  TidyWord2000, 
  TidyWrapAsp, 
  TidyWrapAttVals, 
  TidyWrapJste, 
  TidyWrapPhp, 
  TidyWrapScriptlets, 
  TidyWrapSection, 
  TidyWriteBack,
  TidyXmlDecl,
  TidyXmlPIs,
  TidyXmlSpace,
#if TIDY_APPLE_CHANGES
  TidySanitizeAgainstXSS,
#endif
};

static unsigned int _rndSeed;

static unsigned int xmlFuzzRnd(void) {
    return _rndSeed = (unsigned int)((_rndSeed * 48271UL) % 2147483647UL);  /* C++ std::minstd_rand */
}

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxSize, unsigned int seed) {
    bool optionsMutated = false;
    const size_t tidyOptionBytes = ((sizeof(bool_options)/sizeof(bool_options[0])) + 7) / 8;

    _rndSeed = seed;

    if (size < 1 + tidyOptionBytes)
        return LLVMFuzzerMutate(data, size, maxSize);

    // Mutate input format among xhtml/html/xml (10% chance).
    if (xmlFuzzRnd() % 10 == 1) {
        data[0] = (data[0] & 0xF0) | (((uint8_t)(xmlFuzzRnd() % 3)) & 0x0F);
        optionsMutated = true;
    }

    // Mutate parsing function among tidyParseFile()/tidyParseString()/tidyParseDoc() (10% chance).
    if (xmlFuzzRnd() % 10 == 2) {
        data[0] = ((((uint8_t)(xmlFuzzRnd() % 3)) << 4) & 0xF0) | (data[0] & 0x0F);
        optionsMutated = true;
    }

    // Mutate tidy options (10% chance).
    if (xmlFuzzRnd() % 10 == 3) {
        for (unsigned i = 0; i < tidyOptionBytes; ++i)
            data[1+i] = (uint8_t)xmlFuzzRnd();
        optionsMutated = true;
    }

    // Return without mutating corpus if any options were mutated (10% chance).
    if (optionsMutated && (xmlFuzzRnd() % 10 == 4))
        return size;

    const size_t optionsSize = tidyOptionBytes + 1;
    return optionsSize + LLVMFuzzerMutate(data + optionsSize, size - optionsSize, maxSize - optionsSize);
}

static void set_option(const uint8_t* data, TidyDoc *tdoc, unsigned index) {
  uint8_t decider;
  uint8_t bitmask = (uint8_t)(1U << (index % 8));
  decider = data[(index + 8)/8 - 1U] & bitmask;
  TidyOptionId tboolID = bool_options[index];
  if (decider) tidyOptSetBool( *tdoc, tboolID, yes );
  else { tidyOptSetBool( *tdoc, tboolID, no ); }
}

static int TidyXhtml(const uint8_t* data, size_t size, TidyBuffer* output, TidyBuffer* errbuf) {
  uint8_t decider;

  // We need enough data for picking all of the options.
  const size_t tidyOptionBytes = ((sizeof(bool_options)/sizeof(bool_options[0])) + 7) / 8;
  if (size < 1 + tidyOptionBytes) {
    return 0;
  }

  TidyDoc tdoc = tidyCreate();

  // Decide output format
  decider = *data;
  data++; size--;
  if ((decider & 0x0F) % 3 == 0) tidyOptSetBool( tdoc, TidyXhtmlOut, yes );
  else { tidyOptSetBool( tdoc, TidyXhtmlOut, no ); }

  if ((decider & 0x0F) % 3 == 1) tidyOptSetBool( tdoc, TidyHtmlOut, yes );
  else { tidyOptSetBool( tdoc, TidyHtmlOut, no ); }

  if ((decider & 0x0F) % 3 == 2) tidyOptSetBool( tdoc, TidyXmlOut, yes );
  else { tidyOptSetBool( tdoc, TidyXmlOut, no ); }

  // Set options
  for (unsigned i=0; i < sizeof(bool_options)/sizeof(TidyOptionId); i++) {
    set_option(data, &tdoc, i);
  }
  data += tidyOptionBytes;
  size -= tidyOptionBytes;

  // Set an error buffer.
  tidySetErrorBuffer(tdoc, errbuf);

  // Parse the data
  switch (((decider & 0xF0) >> 4) % 3) {
    case 0: {
      char filename[256];
      snprintf(filename, sizeof(filename), "/tmp/libfuzzer.%d", getpid());

      FILE *fp = fopen(filename, "wb");
      if (!fp) {
          return 0;
      }
      fwrite(data, size, 1, fp);
      fclose(fp);

      if (tidyParseFile(tdoc, filename) >= 0 && tidyCleanAndRepair(tdoc) >= 0 && tidyRunDiagnostics(tdoc) >= 0)
        tidySaveBuffer(tdoc, output);
      unlink(filename);
    }
    break;
    case 1: {
      char *inp = malloc(size+1);
      inp[size] = '\0';
      memcpy(inp, data, size);
      if (tidyParseString(tdoc, inp) >= 0 && tidyCleanAndRepair(tdoc) >= 0 && tidyRunDiagnostics(tdoc) >= 0)
        tidySaveBuffer(tdoc, output);
      free(inp);
    }
    break;
    case 2: {
      TidyBuffer data_buffer;
      tidyBufAttach(&data_buffer, (byte*)data, (unsigned)size);
      if (tidyParseBuffer(tdoc, &data_buffer) >= 0 && tidyCleanAndRepair(tdoc) >= 0 && tidyRunDiagnostics(tdoc) >= 0)
        tidySaveBuffer(tdoc, output);
      tidyBufDetach(&data_buffer);
    }
    break;
  }

  // Cleanup
  tidyRelease( tdoc );

  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  TidyBuffer fuzz_toutput;
  TidyBuffer fuzz_terror;

  tidyBufInit(&fuzz_toutput);
  tidyBufInit(&fuzz_terror);

  TidyXhtml(data, size, &fuzz_toutput, &fuzz_terror);

  tidyBufFree(&fuzz_toutput);
  tidyBufFree(&fuzz_terror);

  return 0;
}
