#ifndef XPYB_COOKIE_H
#define XPYB_COOKIE_H

#include "conn.h"
#include "request.h"
#include "reply.h"

typedef struct {
    PyObject_HEAD
    xpybConn *conn;
    xpybRequest *request;
    PyTypeObject *reply;
    xcb_void_cookie_t cookie;
} xpybCookie;

extern PyTypeObject xpybCookie_type;

int xpybCookie_modinit(PyObject *m);

#endif
