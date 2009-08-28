#ifndef XPYB_REPLY_H
#define XPYB_REPLY_H

#include "response.h"

typedef struct {
    xpybResponse response;
} xpybReply;

extern PyTypeObject xpybReply_type;

int xpybReply_populate(xpybReply *self, xcb_generic_reply_t *data);

int xpybReply_modinit(PyObject *m);

#endif
