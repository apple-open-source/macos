
/* This is stuff to make the code compile with VC++ 6.0
   I know nearly nothing about other versions. */

#include <float.h>
#define isnan _isnan
#define isinf(d) ((_fpclass(d) == _FPCLASS_PINF) ? 1 : ((_fpclass(d) == _FPCLASS_NINF) ? -1 :0))


/*  #ifndef isnan */
/*  int isnan(double number) { return (!(number == number)); } */
/*  #endif */


