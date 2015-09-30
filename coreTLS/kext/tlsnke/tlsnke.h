//
//  tlsnke.h
//  tlsnke
//
//  Created by Fabrice Gautier on 1/12/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#ifndef __TLSNKE_H__
#define __TLSNKE_H__

/* Those should be defined in kernel headers eg <sys/scoket.h> */


#define TLS_HANDLE_IP4 0xBABABABA		/* Temp hack to identify this filter */
#define TLS_HANDLE_IP6 0xABABABAB		/* Temp hack to identify this filter */


/*
SO_TLS_HANDLE: 
Get the DTLS handle used to enable utun to dtls bypass. (getsockopt only) 
option_value type: int
*/
#define SO_TLS_HANDLE 0x20000

/*
SO_TLS_INIT_CIPHER:
Initialize the new cipher key material. (setsockopt only)
option_value type: 
struct {
    uint16_t cipherspec;
    bool server;
    int keylen;
    char key[keylen];
} 
*/
#define SO_TLS_INIT_CIPHER 0x20001

/*
SO_TLS_PROTOCOL_VERSION:
Set the protocol version. (setsockopt only)
option_value type: int
*/
#define SO_TLS_PROTOCOL_VERSION 0x20002

/*
SO_TLS_ADVANCE_READ_CIPHER:
Update the read cipher to use the new key. (setsockopt only)
No option value.
*/
#define SO_TLS_ADVANCE_READ_CIPHER 0x20003

/*
SO_TLS_ADVANCE_WRITE_CIPHER:
Update the write cipher to use the new key. (setsockopt only)
No option value.
*/
#define SO_TLS_ADVANCE_WRITE_CIPHER 0x20004

/*
SO_TLS_ROLLBACK_WRITE_CIPHER: 
Rollback the write cipher to the previous key. (setsockopt only)
No option value.
*/
#define SO_TLS_ROLLBACK_WRITE_CIPHER 0x20005

/*
 SO_TLS_SERVICE_WRITE_QUEUE:
 Service the record write queue
 No option value.
 */
#define SO_TLS_SERVICE_WRITE_QUEUE 0x20006


/* 
SCM_TLS_HEADER: 
 Type of anciallary data for DTLS record header
*/

#define SCM_TLS_HEADER 0x12345

typedef struct tls_record_hdr{
    uint8_t content_type;
    uint16_t protocol_version;
} *tls_record_hdr_t;


#endif /* __TLSNKE_H__ */
