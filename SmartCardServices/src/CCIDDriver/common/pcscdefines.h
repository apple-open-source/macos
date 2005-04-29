/*****************************************************************
/
/ File   :   pcscdefines.h
/ Author :   David Corcoran <corcoran@linuxnet.com>
/ Date   :   June 15, 2000
/ Purpose:   This provides PC/SC shared defines.
/            See http://www.linuxnet.com for more information.
/ License:   See file COPYING.BSD
/
/
******************************************************************/

#ifndef _pcscdefines_h_
#define _pcscdefines_h_

#ifdef __cplusplus
extern "C" {
#endif 


  // do not use RESPONSECODE (long, 64 bits) when 32 bits are enough
  typedef int ifd_t;

  typedef enum {
    STATUS_SUCCESS               = 0xFA,
    STATUS_UNSUCCESSFUL          = 0xFB,
    STATUS_COMM_ERROR            = 0xFC,
    STATUS_DEVICE_PROTOCOL_ERROR = 0xFD
  } status_t;

  #define MAX_RESPONSE_SIZE  264
  #define MAX_ATR_SIZE       33
  #define PCSCLITE_MAX_CHANNELS           16      /* Maximum channels     */
#ifdef __cplusplus
}
#endif

#endif /* _pcscdefines_h_ */
