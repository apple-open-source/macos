/*! @header Enums.h
    @discussion This header tests the supported types of enum declarations.  
*/

/*!
  @enum Beverage Categories Complete
  @discussion Test of anonymous enum with English name of more than one word.
  @constant kSoda Sweet, carbonated, non-alcoholic beverages.
  @constant kBeer Light, grain-based, alcoholic beverages.
  @constant kMilk Dairy beverages.
  @constant kWater Unflavored, non-sweet, non-caloric, non-alcoholic beverages.
*/
  enum {
	kSoda,
	kBeer,
	kMilk,
	kWater
  };

/*!
   Anonymous enum
 */
enum {
	kFoo
};

/*!
	Non-anonymous enum
 */
enum {
	kBar
} bar;
