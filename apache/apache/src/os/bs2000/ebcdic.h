#ifndef AP_EBCDIC_H
#define AP_EBCDIC_H  "$Id: ebcdic.h,v 1.1.1.3 2000/11/09 00:38:56 wsanchez Exp $"

#include <sys/types.h>

extern const unsigned char os_toascii[256];
extern const unsigned char os_toebcdic[256];
API_EXPORT(void *) ebcdic2ascii(void *dest, const void *srce, size_t count);
API_EXPORT(void *) ascii2ebcdic(void *dest, const void *srce, size_t count);

#endif /*AP_EBCDIC_H*/
