#ifndef XPYB_RESPONSE_H
#define XPYB_RESPONSE_H

#include "protobj.h"

typedef struct {
    xpybProtobj base;
} xpybResponse;

extern PyTypeObject xpybResponse_type;

int xpybResponse_modinit(PyObject *m);

#endif
