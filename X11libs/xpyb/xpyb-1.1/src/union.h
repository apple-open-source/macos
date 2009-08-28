#ifndef XPYB_UNION_H
#define XPYB_UNION_H

#include "protobj.h"

typedef struct {
    xpybProtobj base;
} xpybUnion;

extern PyTypeObject xpybUnion_type;

int xpybUnion_modinit(PyObject *m);

#endif
