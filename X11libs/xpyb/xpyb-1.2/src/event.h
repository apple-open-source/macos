#ifndef XPYB_EVENT_H
#define XPYB_EVENT_H

#include "response.h"
#include "conn.h"

typedef struct {
    xpybResponse response;
} xpybEvent;

extern PyTypeObject xpybEvent_type;

PyObject *xpybEvent_create(xpybConn *conn, xcb_generic_event_t *e);

int xpybEvent_modinit(PyObject *m);

#endif
