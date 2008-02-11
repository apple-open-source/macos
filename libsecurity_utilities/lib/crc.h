#ifndef __CRC__
#define __CRC__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

u_int32_t CalculateCRC(u_int8_t* buffer, size_t length);
u_int32_t StagedCRC(u_int32_t initialCRC, u_int8_t* buffer, size_t length);

#ifdef __cplusplus
};
#endif


#endif
