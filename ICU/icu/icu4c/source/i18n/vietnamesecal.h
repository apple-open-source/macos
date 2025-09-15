/*
*****************************************************************************************
* Copyright (C) 2025 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if APPLE_ICU_CHANGES // rdar://24832944

#ifndef VIETCAL_H
#define VIETCAL_H


#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "unicode/timezone.h"
#include "chnsecal.h"

U_NAMESPACE_BEGIN

/**
 * <p><code>VietnameseCalendar</code> is a concrete subclass of {@link Calendar}
 * that implements a traditional Vietnamese lunisolar calendar.</p>
 *
 * <p>VietnameseCalendar usually should be instantiated using 
 * {@link com.ibm.icu.util.Calendar#getInstance(ULocale)} passing in a <code>ULocale</code>
 * with the tag <code>"@calendar=vietnamese"</code>.</p>
 *
 * @internal
 */
class VietnameseCalendar : public ChineseCalendar {
 public:
  //-------------------------------------------------------------------------
  // Constructors...
  //-------------------------------------------------------------------------

  /**
   * Constructs a VietnameseCalendar based on the current time in the default time zone
   * with the given locale.
   *
   * @param aLocale  The given locale.
   * @param success  Indicates the status of VietnameseCalendar object construction.
   *                 Returns U_ZERO_ERROR if constructed successfully.
   * @internal
   */
  VietnameseCalendar(const Locale& aLocale, UErrorCode &success);

  /**
   * Copy Constructor
   * @internal
   */
  VietnameseCalendar(const VietnameseCalendar& other);

  /**
   * Destructor.
   * @internal
   */
  virtual ~VietnameseCalendar();

  /**
   * Clone.
   * @internal
   */
  virtual VietnameseCalendar* clone() const override;

  //----------------------------------------------------------------------
  // Internal methods & astronomical calculations
  //----------------------------------------------------------------------

  /**
   * @return      The related Gregorian year; will be obtained by modifying the value
   *              obtained by get from UCAL_EXTENDED_YEAR field
   * @internal
   */
  virtual int32_t getRelatedYear(UErrorCode &status) const override;

  /**
   * @param year  The related Gregorian year to set; will be modified as necessary then
   *              set in UCAL_EXTENDED_YEAR field
   * @internal
   */
  virtual void setRelatedYear(int32_t year) override;

 private:

  // UObject stuff
 public: 
  /**
   * @return   The class ID for this object. All objects of a given class have the
   *           same class ID. Objects of other classes have different class IDs.
   * @internal
   */
  virtual UClassID getDynamicClassID() const override;

  /**
   * Return the class ID for this class. This is useful only for comparing to a return
   * value from getDynamicClassID(). For example:
   *
   *      Base* polymorphic_pointer = createPolymorphicObject();
   *      if (polymorphic_pointer->getDynamicClassID() ==
   *          Derived::getStaticClassID()) ...
   *
   * @return   The class ID for all objects of this class.
   * @internal
   */
  U_I18N_API static UClassID U_EXPORT2 getStaticClassID();

  /**
   * return the calendar type, "vietnamese".
   *
   * @return calendar type
   * @internal
   */
  const char * getType() const override;

 protected:
  virtual Setting getSetting(UErrorCode& status) const override;

 private:
 
  VietnameseCalendar(); // default constructor not implemented
};

U_NAMESPACE_END

#endif

#endif // VIETCAL_H

#endif // APPLE_ICU_CHANGES
