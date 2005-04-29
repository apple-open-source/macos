/*
 * x509.h
 *
 * Copyright (C) AB Strakt 2001, All rights reserved
 *
 * Export x509 functions and data structure.
 * See the file RATIONALE for a short explanation of why this module was written.
 *
 * Reviewed 2001-07-23
 *
 * @(#) $Id: x509.h,v 1.2 2004/09/23 14:25:28 murata Exp $
 */
#ifndef PyOpenSSL_crypto_X509_H_
#define PyOpenSSL_crypto_X509_H_

#include <Python.h>
#include <openssl/ssl.h>

extern  int       init_crypto_x509   (PyObject *);

extern  PyTypeObject      crypto_X509_Type;

#define crypto_X509_Check(v) ((v)->ob_type == &crypto_X509_Type)

typedef struct {
    PyObject_HEAD
    X509                *x509;
    int                  dealloc;
} crypto_X509Obj;


#endif
