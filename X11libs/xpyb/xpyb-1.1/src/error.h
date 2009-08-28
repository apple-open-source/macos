#ifndef XPYB_ERROR_H
#define XPYB_ERROR_H

#include "response.h"
#include "conn.h"

typedef struct {
    xpybResponse response;
} xpybError;

extern PyTypeObject xpybError_type;

int xpybError_set(xpybConn *conn, xcb_generic_error_t *e);

int xpybError_modinit(PyObject *m);

#endif
