/*
 * See fips186prf.c for copyright information
 */
#ifndef _FIPS186PRF_H
#define _FIPS186PRF_H
/*
 * FIPS 186-2 PRF based upon SHA1.
 */
extern void fips186_2prf(uint8_t mk[20], uint8_t finalkey[160]);
#endif /* _FIPS186PRF_H */
