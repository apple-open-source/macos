/* version information

  (c) 2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author: iccir $ 
    $Date: 2007/02/03 02:31:30 $ 
    $Revision: 1.2 $ 

*/

/* Apple Inc. Changes:
   2007-01-29 iccir Added Apple-specific build information
*/
#ifdef TIDY_APPLE_CHANGES
static const char TY_(release_date)[] = "31 October 2006" " - Apple Inc. build " TIDY_APPLE_BUILD_NUMBER_STR;
#else
static const char TY_(release_date)[] = "31 October 2006";
#endif
