/**
 ************************************************************************************
 * Copyright (C) 2006-2007, International Business Machines Corporation and others. *
 * All Rights Reserved.                                                             *
 ************************************************************************************
 */

#ifndef AAPLBFCT_H
#define AAPLBFCT_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/utext.h"
#include "unicode/uscript.h"
#include "brkeng.h"

U_NAMESPACE_BEGIN

class AppleLanguageBreakFactory : public ICULanguageBreakFactory {
 public:

  /**
   * <p>Standard constructor.</p>
   *
   */
  AppleLanguageBreakFactory(UErrorCode &status);

  /**
   * <p>Virtual destructor.</p>
   */
  virtual ~AppleLanguageBreakFactory();

 protected:

 /**
  * <p>Create a CompactTrieDictionary for the specified script and break type.</p>
  *
  * @param script A script code that identifies the dictionary to be
  * created.
  * @param breakType The kind of text break for which a dictionary is
  * sought.
  * @return A CompactTrieDictionary with the desired characteristics, or 0.
  */
  virtual const CompactTrieDictionary *loadDictionaryFor(UScriptCode script, int32_t breakType);

};

U_NAMESPACE_END

    /* AAPLBFCT_H */
#endif
