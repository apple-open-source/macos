#ifndef XPYB_VOID_H
#define XPYB_VOID_H

#include "cookie.h"

typedef struct {
    xpybCookie cookie;
} xpybVoid;

extern PyTypeObject xpybVoid_type;

int xpybVoid_modinit(PyObject *m);

#endif
