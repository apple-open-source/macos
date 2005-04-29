#ifndef __CRC32_H__
#define __CRC32_H__

#ifndef _ANSI_ARGS_
#ifdef PROTOTYPES
#define _ANSI_ARGS_(c)	c
#else
#define _ANSI_ARGS_(c)	()
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long crc32_t;
#define Z_NULL  0

crc32_t crc32 _ANSI_ARGS_((crc32_t crc, const unsigned char *buf, unsigned int len));
/*
     Update a running crc with the bytes buf[0..len-1] and return the updated
   crc. If buf is NULL, this function returns the required initial value
   for the crc. Pre- and post-conditioning (one's complement) is performed
   within this function so it shouldn't be done by the application.
   Usage example:

     uLong crc = crc32(0L, Z_NULL, 0);

     while (read_buffer(buffer, length) != EOF) {
       crc = crc32(crc, buffer, length);
     }
     if (crc != original_crc) error();
*/

#ifdef __cplusplus
}
#endif
#endif
