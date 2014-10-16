/* version information

  (c) 2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

/* Apple Inc. Changes:
   2007-01-29 iccir Added Apple-specific build information
*/
#ifdef TIDY_APPLE_CHANGES
static const char TY_(release_date)[] = "31 October 2006" " - Apple Inc. build " TIDY_APPLE_BUILD_NUMBER_STR;
#else
static const char TY_(release_date)[] = "31 October 2006";
#endif
