/*
 * MatsUtils.h ---
 *
 * 		Some handy utility functions.
 */

#define BitStringFromUInt32(flag, str) 		BitStringFromX((unsigned int) flag, 32, str)
#define BitStringFromUShort16(flag, str) 	BitStringFromX((unsigned short) flag, 16, str)

void BitStringFromX( unsigned int flag, int n, char *str );
