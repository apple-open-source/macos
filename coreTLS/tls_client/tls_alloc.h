//
//  tls_alloc.h
//  coretls_tools
//

#ifndef __TLS_ALLOC_H__
#define __TLS_ALLOC_H__

int mySSLAlloc(tls_buffer *buf, size_t len);
void mySSLFree(tls_buffer *buf);

#endif /* __TLS_ALLOC_H__ */
