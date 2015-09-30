//
//  tls_alloc.c
//  coretls_tools
//

#include <stdlib.h>
#include <tls_types.h>
#include <sys/errno.h>

#include "tls_alloc.h"

int mySSLAlloc(tls_buffer *buf, size_t len)
{
    buf->data=malloc(len);
    if(!buf->data)
        return ENOMEM;
    buf->length=len;
    return 0;
}

void mySSLFree(tls_buffer *buf)
{
    if(buf->data)
        free(buf->data);
    buf->data=NULL;
    buf->length=0;
}


