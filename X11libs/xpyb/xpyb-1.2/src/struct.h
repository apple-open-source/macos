#ifndef XPYB_STRUCT_H
#define XPYB_STRUCT_H

#include "protobj.h"

typedef struct {
    xpybProtobj base;
} xpybStruct;

extern PyTypeObject xpybStruct_type;

int xpybStruct_modinit(PyObject *m);

#endif
