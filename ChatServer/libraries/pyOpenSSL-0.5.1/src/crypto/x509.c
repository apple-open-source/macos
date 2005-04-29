/*
 * x509.c
 *
 * Copyright (C) AB Strakt 2001, All rights reserved
 *
 * Certificate (X.509) handling code, mostly thin wrappers around OpenSSL.
 * See the file RATIONALE for a short explanation of why this module was written.
 *
 * Reviewed 2001-07-23
 */
#include <Python.h>
#define crypto_MODULE
#include "crypto.h"

static char *CVSid = "@(#) $Id: x509.c,v 1.2 2004/09/23 14:25:28 murata Exp $";

/* 
 * X.509 is a standard for digital certificates.  See e.g. the OpenSSL homepage
 * http://www.openssl.org/ for more information
 */

static char crypto_X509_get_version_doc[] = "\n\
Return version number of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   Version number as a Python integer\n\
";

static PyObject *
crypto_X509_get_version(crypto_X509Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_version"))
        return NULL;

    return PyInt_FromLong((long)X509_get_version(self->x509));
}

static char crypto_X509_set_version_doc[] = "\n\
Set version number of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             version - The version number\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_set_version(crypto_X509Obj *self, PyObject *args)
{
    int version;

    if (!PyArg_ParseTuple(args, "i:set_version", &version))
        return NULL;

    X509_set_version(self->x509, version);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_serial_number_doc[] = "\n\
Return serial number of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   Serial number as a Python integer\n\
";

static PyObject *
crypto_X509_get_serial_number(crypto_X509Obj *self, PyObject *args)
{
    ASN1_INTEGER *asn1_i;

    if (!PyArg_ParseTuple(args, ":get_serial_number"))
        return NULL;

    asn1_i = X509_get_serialNumber(self->x509);
    return PyInt_FromLong(ASN1_INTEGER_get(asn1_i));
}

static char crypto_X509_set_serial_number_doc[] = "\n\
Set serial number of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             serial - The serial number\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_set_serial_number(crypto_X509Obj *self, PyObject *args)
{
    long serial;

    if (!PyArg_ParseTuple(args, "l:set_serial_number", &serial))
        return NULL;

    ASN1_INTEGER_set(X509_get_serialNumber(self->x509), serial);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_issuer_doc[] = "\n\
Create an X509Name object for the issuer of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   An X509Name object\n\
";

static PyObject *
crypto_X509_get_issuer(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *pyname;
    X509_NAME *name;

    if (!PyArg_ParseTuple(args, ":get_issuer"))
        return NULL;

    name = X509_get_issuer_name(self->x509);
    pyname = crypto_X509Name_New(name, 0);
    if (pyname != NULL)
    {
        pyname->parent_cert = (PyObject *)self;
        Py_INCREF(self);
    }
    return (PyObject *)pyname;
}

static char crypto_X509_set_issuer_doc[] = "\n\
Set the issuer of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             issuer - The issuer name\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_set_issuer(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *issuer;

    if (!PyArg_ParseTuple(args, "O!:set_issuer", &crypto_X509Name_Type,
			  &issuer))
        return NULL;

    if (!X509_set_issuer_name(self->x509, issuer->x509_name))
    {
        exception_from_error_queue();
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_subject_doc[] = "\n\
Create an X509Name object for the subject of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   An X509Name object\n\
";

static PyObject *
crypto_X509_get_subject(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *pyname;
    X509_NAME *name;

    if (!PyArg_ParseTuple(args, ":get_subject"))
        return NULL;

    name = X509_get_subject_name(self->x509);
    pyname = crypto_X509Name_New(name, 0);
    if (pyname != NULL)
    {
        pyname->parent_cert = (PyObject *)self;
        Py_INCREF(self);
    }
    return (PyObject *)pyname;
}

static char crypto_X509_set_subject_doc[] = "\n\
Set the subject of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             subject - The subject name\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_set_subject(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *subject;

    if (!PyArg_ParseTuple(args, "O!:set_subject", &crypto_X509Name_Type,
			  &subject))
        return NULL;

    if (!X509_set_subject_name(self->x509, subject->x509_name))
    {
        exception_from_error_queue();
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_pubkey_doc[] = "\n\
Get the public key of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   The public key\n\
";

static PyObject *
crypto_X509_get_pubkey(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *crypto_PKey_New(EVP_PKEY *, int);
    EVP_PKEY *pkey;

    if (!PyArg_ParseTuple(args, ":get_pubkey"))
        return NULL;

    if ((pkey = X509_get_pubkey(self->x509)) == NULL)
    {
        exception_from_error_queue();
        return NULL;
    }

    return (PyObject *)crypto_PKey_New(pkey, 0);
}

static char crypto_X509_set_pubkey_doc[] = "\n\
Set the public key of the certificate\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             pkey - The public key\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_set_pubkey(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *pkey;

    if (!PyArg_ParseTuple(args, "O!:set_pubkey", &crypto_PKey_Type, &pkey))
        return NULL;

    if (!X509_set_pubkey(self->x509, pkey->pkey))
    {
        exception_from_error_queue();
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_gmtime_adj_notBefore_doc[] = "\n\
Adjust the time stamp for when the certificate starts being valid\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             i - The adjustment\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_gmtime_adj_notBefore(crypto_X509Obj *self, PyObject *args)
{
    long i;

    if (!PyArg_ParseTuple(args, "l:gmtime_adj_notBefore", &i))
        return NULL;

    X509_gmtime_adj(X509_get_notBefore(self->x509), i);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_gmtime_adj_notAfter_doc[] = "\n\
Adjust the time stamp for when the certificate stops being valid\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             i - The adjustment\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_gmtime_adj_notAfter(crypto_X509Obj *self, PyObject *args)
{
    long i;

    if (!PyArg_ParseTuple(args, "l:gmtime_adj_notAfter", &i))
        return NULL;

    X509_gmtime_adj(X509_get_notAfter(self->x509), i);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_sign_doc[] = "\n\
Sign the certificate using the supplied key and digest\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be:\n\
             pkey   - The key to sign with\n\
             digest - The message digest to use\n\
Returns:   None\n\
";

static PyObject *
crypto_X509_sign(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *pkey;
    char *digest_name;
    const EVP_MD *digest;

    if (!PyArg_ParseTuple(args, "O!s:sign", &crypto_PKey_Type, &pkey,
			  &digest_name))
        return NULL;

    if ((digest = EVP_get_digestbyname(digest_name)) == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "No such digest method");
        return NULL;
    }

    if (!X509_sign(self->x509, pkey->pkey, digest))
    {
        exception_from_error_queue();
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_has_expired_doc[] = "\n\
Check whether the certificate has expired.\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   True if the certificate has expired, false otherwise\n\
";

static PyObject *
crypto_X509_has_expired(crypto_X509Obj *self, PyObject *args)
{
    time_t tnow;

    if (!PyArg_ParseTuple(args, ":has_expired"))
        return NULL;

    tnow = time(NULL);
    if (ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(self->x509), tnow) < 0)
        return PyInt_FromLong(1L);
    else
        return PyInt_FromLong(0L);
}


/*
 * ADD_METHOD(name) expands to a correct PyMethodDef declaration
 *   {  'name', (PyCFunction)crypto_X509_name, METH_VARARGS }
 * for convenience
 */
#define ADD_METHOD(name)        \
    { #name, (PyCFunction)crypto_X509_##name, METH_VARARGS, crypto_X509_##name##_doc }
static PyMethodDef crypto_X509_methods[] =
{
    ADD_METHOD(get_version),
    ADD_METHOD(set_version),
    ADD_METHOD(get_serial_number),
    ADD_METHOD(set_serial_number),
    ADD_METHOD(get_issuer),
    ADD_METHOD(set_issuer),
    ADD_METHOD(get_subject),
    ADD_METHOD(set_subject),
    ADD_METHOD(get_pubkey),
    ADD_METHOD(set_pubkey),
    ADD_METHOD(gmtime_adj_notBefore),
    ADD_METHOD(gmtime_adj_notAfter),
    ADD_METHOD(sign),
    ADD_METHOD(has_expired),
    { NULL, NULL }
};
#undef ADD_METHOD


/*
 * Constructor for X509 objects, never called by Python code directly
 *
 * Arguments: cert    - A "real" X509 certificate object
 *            dealloc - Boolean value to specify whether the destructor should
 *                      free the "real" X509 object
 * Returns:   The newly created X509 object
 */
crypto_X509Obj *
crypto_X509_New(X509 *cert, int dealloc)
{
    crypto_X509Obj *self;

    self = PyObject_New(crypto_X509Obj, &crypto_X509_Type);

    if (self == NULL)
        return NULL;

    self->x509 = cert;
    self->dealloc = dealloc;

    return self;
}

/*
 * Deallocate the memory used by the X509 object
 *
 * Arguments: self - The X509 object
 * Returns:   None
 */
static void
crypto_X509_dealloc(crypto_X509Obj *self)
{
    /* Sometimes we don't have to dealloc the "real" X509 pointer ourselves */
    if (self->dealloc)
        X509_free(self->x509);

    PyObject_Del(self);
}

/*
 * Find attribute
 *
 * Arguments: self - The X509 object
 *            name - The attribute name
 * Returns:   A Python object for the attribute, or NULL if something went
 *            wrong
 */
static PyObject *
crypto_X509_getattr(crypto_X509Obj *self, char *name)
{
    return Py_FindMethod(crypto_X509_methods, (PyObject *)self, name);
}

PyTypeObject crypto_X509_Type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "X509",
    sizeof(crypto_X509Obj),
    0,
    (destructor)crypto_X509_dealloc,
    NULL, /* print */
    (getattrfunc)crypto_X509_getattr,
};

/*
 * Initialize the X509 part of the crypto sub module
 *
 * Arguments: dict - The crypto module dictionary
 * Returns:   None
 */
int
init_crypto_x509(PyObject *dict)
{
    crypto_X509_Type.ob_type = &PyType_Type;
    Py_INCREF(&crypto_X509_Type);
    PyDict_SetItemString(dict, "X509Type", (PyObject *)&crypto_X509_Type);
    return 1;
}

