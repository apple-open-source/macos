/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_CRC_H_
#define _SECURITY_AUTH_CRC_H_

#if defined(__cplusplus)
extern "C" {
#endif

extern const uint64_t _crc_table64[256];
extern const uint64_t xorout;
    
AUTH_INLINE uint64_t
crc64_init()
{
    return xorout;
}

AUTH_INLINE uint64_t
crc64_final(uint64_t crc)
{
      return crc ^= xorout;
}
    
AUTH_INLINE AUTH_NONNULL_ALL uint64_t
crc64_update(uint64_t crc, const void *buf, uint64_t len)
{
    const unsigned char * ptr = (const unsigned char *) buf;

    while (len-- > 0) {
        crc = _crc_table64[((crc >> 56) ^ *(ptr++)) & 0xff] ^ (crc << 8);
    }
    
    return crc;
}

AUTH_INLINE uint64_t
crc64(const void *buf, uint64_t len)
{
    uint64_t crc = crc64_init();
    
    crc = crc64_update(crc, buf, len);
    
    crc = crc64_final(crc);
    
    return crc;
}
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_CRC_H_ */
