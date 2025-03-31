// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *****************************************************************************
 * Copyright (C) 2007-2023, International Business Machines Corporation
 * and others. All Rights Reserved.
 *****************************************************************************
 *
 * File CHNSECAL.H
 *
 * Modification History:
 *
 *
 *****************************************************************************
 */

#ifndef HINDUCAL_H
#define HINDUCAL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "unicode/timezone.h"

#include <iostream>
#include <cmath>
#include <vector>

U_NAMESPACE_BEGIN

  class Angle {
      public:
      	int32_t degrees;
        int32_t minutes;
        int32_t seconds;
  };

  class Location {
      public:
        Angle latitude;
        Angle longitude;
        int32_t meters;
        double hours;
  };

  struct HinduDate {
      int32_t year;
      int32_t month;
      bool leapMonth;
      int32_t day;
      bool leapDay;

      public:
      	HinduDate(int32_t y, int32_t m, bool lM, int32_t d, bool lD) {
      		year = y; month = m; leapMonth = lM; day = d; leapDay = lD;
      	}
  };

/**
 * <code>HinduLunarCalendar</code> is a concrete subclass of {@link Calendar}
 * that implements a traditional Hindu lunisolar calendar.  
 *
 * <p>References:<ul>
 *
 * <li>Dershowitz and Reingold, <i>Calendrical Calculations</i>,
 * Cambridge University Press, 1997</li>
 
 *
 * </ul>
 *
 * <p>
 * This class should only be subclassed to implement variants of the Hindu lunar calendar.</p>
 * <p>
 * HinduLunarCalendar usually should be instantiated using
 * {@link com.ibm.icu.util.Calendar#getInstance(ULocale)} passing in a <code>ULocale</code>
 * with the tag <code>"@calendar=hindu-lunar"</code>.</p>
 *
 * @see com.ibm.icu.util.Calendar
 * @author Dragan Besevic
 * @internal
 */
class U_I18N_API HinduLunarCalendar : public Calendar {
 public:
  ///////////////////////////////////////////////////////////////
  // Constructors
  ///////////////////////////////////////////////////////////////

  /**
   * Constructs a HinduLunarCalendar based on the current time in the default time zone
   * with the given locale.
   *
   * @param aLocale  The given locale.
   * @param success  Indicates the status of HinduLunarCalendar object construction.
   *                 Returns U_ZERO_ERROR if constructed successfully.
   * @internal
   */
  HinduLunarCalendar(const Locale& aLocale, UErrorCode &success);

 protected:

   /**
   * Constructs a HinduLunarCalendar based on the current time in the default time zone
   * with the given locale, using the specified epoch year and time zone for
   * astronomical calculations.
   *
   * @param aLocale         The given locale.
   * @param epochYear       The epoch year to use for calculation.
   * @param zoneAstroCalc   The TimeZone to use for astronomical calculations. If null,
   *                        will be set appropriately for Hindu calendar (UTC + 5:30).
   * @param success         Indicates the status of HinduLunarCalendar object construction;
   *                        if successful, will not be changed to an error value.
   * @internal
   */
  HinduLunarCalendar(const Locale& aLocale, int32_t epochYear, const TimeZone* zoneAstroCalc, UErrorCode &success);

 public:
  /**
   * Copy Constructor
   * @internal
   */
  HinduLunarCalendar(const HinduLunarCalendar& other);

  /**
   * Destructor.
   * @internal
   */
  virtual ~HinduLunarCalendar();

  // clone
  virtual HinduLunarCalendar* clone() const override;
    
  /*
   * Is leap day, a Hindu calendar specific
   */
 UBool isLeapDay() const;

 private:

  ///////////////////////////////////////////////////////////////
  // Internal data....
  ///////////////////////////////////////////////////////////////

  UBool isLeapYear;
  UBool leapDay;
  Location hinduLocation;
  int32_t fEpochYear;   // Start year of this Hindu calendar instance.
  const TimeZone* fZoneAstroCalc;   // Zone used for the astronomical calculation
                                    // of this Hindu calendar instance.

  ///////////////////////////////////////////////////////////////
  // Calendar framework
  ///////////////////////////////////////////////////////////////

 protected:
  virtual int32_t handleGetLimit(UCalendarDateFields field, ELimitType limitType) const override;
  virtual int32_t handleGetMonthLength(int32_t extendedYear, int32_t month, UErrorCode& status) const override;
  virtual int64_t handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth, UErrorCode& status) const override;
  virtual int32_t handleGetExtendedYear(UErrorCode& status) override;
  virtual void handleComputeFields(int32_t julianDay, UErrorCode &status) override;
  virtual const UFieldResolutionTable* getFieldResolutionTable() const override;

 public:
  virtual void add(UCalendarDateFields field, int32_t amount, UErrorCode &status) override;
  virtual void add(EDateFields field, int32_t amount, UErrorCode &status) override;
  virtual void roll(UCalendarDateFields field, int32_t amount, UErrorCode &status) override;
  virtual void roll(EDateFields field, int32_t amount, UErrorCode &status) override;

  ///////////////////////////////////////////////////////////////
  // Internal methods & astronomical calculations
  ///////////////////////////////////////////////////////////////

 private:

  static const UFieldResolutionTable HINDU_DATE_PRECEDENCE[];

  const TimeZone* getHinduCalZoneAstroCalc(void) const;
  UBool isLeapMonthBetween(int32_t newMoon1, int32_t newMoon2) const;
  void setLocation(const Angle& latitude, const Angle& longitude, int32_t meters, double hours);

  // Ctor
  HinduDate hinduDate(int32_t year, int32_t month, bool leapMonth, int32_t day, bool leapDay) const;
  HinduDate hinduLunarFromFixed(int32_t date) const;  // Hindu lunar date, new-moon scheme, equivalent to fixed date.
  HinduDate hinduFullmoonFromFixed(int32_t date) const; // Hindu lunar date, full-moon scheme, equivalent to fixed date.

  // APIs
  int32_t fixedFromHinduLunar(HinduDate lDate) const;  // Fixed date corresponding to Hindu lunar date lDate.
  int32_t hinduDayCount(int32_t date) const;  // Elapsed days (Ahargana) to date since Hindu epoch (KY)
  int32_t hinduLunarNewYear(int32_t gYear) const; // Fixed date of Hindu lunisolar new year in a given Gregorian year

  
    // Calendar helpers
  int32_t gregorianNewYear(int32_t gYear) const;
  std::pair<int32_t,int32_t> gregorianYearRange(int32_t gYear) const;
  
  // Internal hindu
protected:
  double ujjainOffset() const;

  ///////////////////////////////////////////////////////////////
  //      Math and algo helpers
  ///////////////////////////////////////////////////////////////
private:
  // Trigonometry
  double angle(int32_t d, int32_t m, double s) const; // Function to calculate an angle in degrees from degrees, arcminutes, and arcseconds

  int32_t amod(int32_t x, int32_t y) const;
  double mod3(double x, double a, double b) const;
  double modLisp(double a, double b) const;

  // Vector helpers
  double getBegin(const std::pair<double, double>& range) const;
  double getEnd(const std::pair<double, double>& range) const;
  bool isInRange(double tee, const std::pair<double, double>& range) const;
  std::vector<int32_t> listRange(const std::vector<int32_t>& ell, const std::pair<int32_t, int32_t>& range) const;

  std::pair<int32_t, int32_t> createInterval(int32_t t0, int32_t t1) const;



  // UObject stuff
 public:
  /**
   * @return   The class ID for this object. All objects of a given class have the
   *           same class ID. Objects of other classes have different class IDs.
   * @internal
   */
  virtual UClassID getDynamicClassID(void) const override;

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
  static UClassID U_EXPORT2 getStaticClassID(void);

  /**
   * return the calendar type, "hindu-lunar".
   *
   * @return calendar type
   * @internal
   */
  virtual const char * getType() const override;


 protected:
  /**
   * (Overrides Calendar) Return true if the current date for this Calendar is in
   * Daylight Savings Time. Recognizes DST_OFFSET, if it is set.
   *
   * @param status Fill-in parameter which receives the status of this operation.
   * @return   True if the current date for this Calendar is in Daylight Savings Time,
   *           false, otherwise.
   * @internal
   */
  virtual UBool inDaylightTime(UErrorCode& status) const override;


  /**
   * Returns true because the Hindu Calendar does have a default century
   * @internal
   */
  virtual UBool haveDefaultCentury() const override;

  /**
   * Returns the date of the start of the default century
   * @return start of century - in milliseconds since epoch, 1970
   * @internal
   */
  virtual UDate defaultCenturyStart() const override;

  /**
   * Returns the year in which the default century begins
   * @internal
   */
  virtual int32_t defaultCenturyStartYear() const override;

 private: // default century stuff.

  /**
   * Returns the beginning date of the 100-year window that dates
   * with 2-digit years are considered to fall within.
   */
  UDate         internalGetDefaultCenturyStart(void) const;

  /**
   * Returns the first year of the 100-year window that dates with
   * 2-digit years are considered to fall within.
   */
  int32_t          internalGetDefaultCenturyStartYear(void) const;

  HinduLunarCalendar() = delete; // default constructor not implemented

 public:


};

/*
 * HinduSolarCalendar
 *
 */
class U_I18N_API HinduSolarCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an HinduSolarCalendar based on the current time in the default time zone
     * with the given locale.
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduSolarCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduSolarCalendar(const HinduSolarCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduSolarCalendar();
    
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
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "hindu-solar".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduSolarCalendar* clone() const override;
    
    ///////////////////////////////////////////////////////////////
    // Calendar framework
    ///////////////////////////////////////////////////////////////
    
protected:
    virtual void handleComputeFields(int32_t julianDay, UErrorCode &status) override;
    
    ///////////////////////////////////////////////////////////////
    // Internal methods & astronomical calculations
    ///////////////////////////////////////////////////////////////

   private:
    HinduDate hinduSolarFromFixed(int32_t date) const;  // Hindu solar date, new-moon scheme, equivalent to fixed date.
    int nextMonthStart(double critical, int approx) const;
};

/*
 * Hindu Lunisolar Vikram Calendar
 *
 */
class U_I18N_API HinduLunisolarVikramCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an Hindu lunisolar calendar based on Vikram calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduLunisolarVikramCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduLunisolarVikramCalendar(const HinduLunisolarVikramCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduLunisolarVikramCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "vikram".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduLunisolarVikramCalendar* clone() const override;
};

/*
 * Hindu Lunisolar Gujarati Calendar
 *
 */
class U_I18N_API HinduLunisolarGujaratiCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an Hindu lunisolar calendar based on Gujarati calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduLunisolarGujaratiCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduLunisolarGujaratiCalendar(const HinduLunisolarGujaratiCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduLunisolarGujaratiCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "gujarati".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduLunisolarGujaratiCalendar* clone() const override;
};

/*
 * Hindu Lunisolar Kannada Calendar
 *
 */
class U_I18N_API HinduLunisolarKannadaCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an Hindu lunisolar calendar based on Kannada calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduLunisolarKannadaCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduLunisolarKannadaCalendar(const HinduLunisolarKannadaCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduLunisolarKannadaCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "kannada".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduLunisolarKannadaCalendar* clone() const override;
};

/*
 * Hindu Lunisolar Marathi Calendar
 *
 */
class U_I18N_API HinduLunisolarMarathiCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an Hindu lunisolar calendar based on Marathi calendrical calculations t
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduLunisolarMarathiCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduLunisolarMarathiCalendar(const HinduLunisolarMarathiCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduLunisolarMarathiCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "marathi".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduLunisolarMarathiCalendar* clone() const override;
};

/*
 * Hindu Lunisolar Telugu Calendar
 *
 */
class U_I18N_API HinduLunisolarTeluguCalendar : public HinduLunarCalendar {
public:
    /**
     * Constructs an Hindu lunisolar calendar based on Telugu calendrical calculations t
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduLunisolarTeluguCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduLunisolarTeluguCalendar(const HinduLunisolarTeluguCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduLunisolarTeluguCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "telugu".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduLunisolarTeluguCalendar* clone() const override;
};


/*
 * Hindu Solar Bangla Calendar
 *
 */
class U_I18N_API HinduSolarBanglaCalendar : public HinduSolarCalendar {
public:
    /**
     * Constructs an Hindu solar calendar based on Bangla calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduSolarBanglaCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduSolarBanglaCalendar(const HinduSolarBanglaCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduSolarBanglaCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "bangla".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduSolarBanglaCalendar* clone() const override;
};

/*
 * Hindu Solar Malayalam Calendar
 *
 */
class U_I18N_API HinduSolarMalayalamCalendar : public HinduSolarCalendar {
public:
    /**
     * Constructs an Hindu solar calendar based on Bengali calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduSolarMalayalamCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduSolarMalayalamCalendar(const HinduSolarMalayalamCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduSolarMalayalamCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "malayalam".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduSolarMalayalamCalendar* clone() const override;
};

/*
 * Hindu Solar Odia Calendar
 *
 */
class U_I18N_API HinduSolarOdiaCalendar : public HinduSolarCalendar {
public:
    /**
     * Constructs an Hindu solar calendar based on Bengali calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduSolarOdiaCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduSolarOdiaCalendar(const HinduSolarOdiaCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduSolarOdiaCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "odia".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduSolarOdiaCalendar* clone() const override;
};

/*
 * Hindu Solar Tamil Calendar
 *
 */
class U_I18N_API HinduSolarTamilCalendar : public HinduSolarCalendar {
public:
    /**
     * Constructs an Hindu solar calendar based on Bengali calendrical calculations
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of HinduSolarCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    HinduSolarTamilCalendar(const Locale& aLocale, UErrorCode &success);
    
    /**
     * Copy Constructor
     * @internal
     */
    HinduSolarTamilCalendar(const HinduSolarTamilCalendar& other) = default;
    
    /**
     * Destructor.
     * @internal
     */
    virtual ~HinduSolarTamilCalendar();
    
    /**
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID() const override;
    
    /**
     * Return the class ID for this class
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    static UClassID U_EXPORT2 getStaticClassID();
    
    /**
     * return the calendar type, "tamil".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const override;
    
    // clone
    virtual HinduSolarTamilCalendar* clone() const override;
};



U_NAMESPACE_END

#endif
#endif
