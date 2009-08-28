#ifndef XPYB_EXT_H
#define XPYB_EXT_H

#include "conn.h"
#include "extkey.h"

typedef struct {
    PyObject_HEAD
    xpybExtkey *key;
    xpybConn *conn;
    unsigned char present;
    unsigned char major_opcode;
    unsigned char first_event;
    unsigned char first_error;
} xpybExt;

extern PyTypeObject xpybExt_type;

int xpybExt_modinit(PyObject *m);

#endif
