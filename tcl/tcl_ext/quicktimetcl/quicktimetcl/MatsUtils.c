/*
 * MatsUtils.c ---
 *
 * 		Some handy utility functions.
 *
 *
 */


#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include	"MatsUtils.h"

void BitStringFromX( unsigned int flag, int n, char *str )
{
	int		i;
	
	str[n] = '\0';
	for (i = 0; i < n; i++, flag >>= 1)
		str[n-1-i] = ((flag & 0x0001) == 0) ? '0' : '1';
}

