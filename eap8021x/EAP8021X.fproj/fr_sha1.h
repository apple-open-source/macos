#ifndef _FR_SHA1_H
#define _FR_SHA1_H

#include <stdint.h>
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} fr_SHA1_CTX;

void fr_SHA1Transform(fr_SHA1_CTX * context, const uint8_t buffer[64]);
void fr_SHA1Init(fr_SHA1_CTX* context);
#if 0
void fr_SHA1Update(fr_SHA1_CTX* context, const uint8_t* data, unsigned int len);
void fr_SHA1Final(uint8_t digest[20], fr_SHA1_CTX* context);
#endif 0

/*
 * this version implements a raw SHA1 transform, no length is appended,
 * nor any 128s out to the block size.
 */
void fr_SHA1FinalNoLen(uint8_t digest[20], fr_SHA1_CTX* context);

#endif /* _FR_SHA1_H */
