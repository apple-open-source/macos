#ifndef XPYB_REQUEST_H
#define XPYB_REQUEST_H

#include "protobj.h"

typedef struct {
    xpybProtobj base;
    int is_void;
    int is_checked;
    unsigned char opcode;
} xpybRequest;

extern PyTypeObject xpybRequest_type;

int xpybRequest_modinit(PyObject *m);

#endif
