#ifndef SMBENCRYPT_H
#define SMBENCRYPT_H

#include "smbtypes.h"

#ifdef __cplusplus
extern "C" {
#endif
void SMBencrypt(uint8 *passwd, uint8 *c8, uint8 *p24);
void SMBNTencrypt(uint8 *passwd, uint8 *c8, uint8 *p24);
#ifdef __cplusplus
}

#endif
#endif
