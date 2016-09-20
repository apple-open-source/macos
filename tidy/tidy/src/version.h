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
   2016-02-25 ddkilzer FIXME: Need to set TIDY_APPLE_BUILD_NUMBER[STR] macros
              in Visual C++ project based on $(RC_ProjectSourceVersion)
              environment variable for Windows.
*/
#ifdef __APPLE__
static const char TY_(release_date)[] = "31 October 2006" " - Apple Inc. build " TIDY_APPLE_BUILD_NUMBER_STR;
#else
static const char TY_(release_date)[] = "31 October 2006";
#endif
