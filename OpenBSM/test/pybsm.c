/*-
 * Copyright (c) 2008 Stacey D. Son <sson@FreeBSD.org>
 * Copyright (c) 2008 Apple, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $P4$
 */

#include <errno.h>
#include <fcntl.h>
#include <Python.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <arpa/inet.h>

#include <bsm/audit.h>
#include <bsm/libbsm.h>
/* XXXss -  audit_ioctl.h doesn't get installed by the kernel
#include <security/audit/audit_ioctl.h>
*/
#include "audit_ioctl.h"

struct io_ctx {
	int 		 io_fd;
	FILE		*io_fp;
	u_int32_t 	 io_flags;
};

#define IOFLAG_AUDITFILE	0x0000
#define IOFLAG_AUDITPIPE	0x0001

struct token_py {
	PyObject 		*t_po;
	STAILQ_ENTRY(token_py)	 tokens_py;
}; 

STAILQ_HEAD(token_head_py, token_py);

/*
 * pybsm methods.
 */
PyDoc_STRVAR(au_get_cond__doc__,
"au_get_cond() -> 'AUC_AUDITING', 'AUE_NOAUDIT' or 'AUC_DISABLED'          \n\
                                                                           \n\
                                                                           \n\
Get the current audit condition. See auditon(2) for more information.");

PyDoc_STRVAR(au_set_cond__doc__,
"au_set_cond('AUC_AUDITING', 'AUE_NOAUDIT' or 'AUC_DISABLED')              \n\
                                                                           \n\
                                                                           \n\
Set the current audit condition. See auditon(2) for more information.");

PyDoc_STRVAR(au_get_policy__doc__,
"au_get_policy() -> ('AUDIT_CNT', 'AUDIT_AHLT', 'AUDIT_ARGV', 'AUDIT_ARGE')\n\
                                                                           \n\
                                                                           \n\
Get the audit policy flags. See auditon(2) for more information.");

PyDoc_STRVAR(au_set_policy__doc__,
"au_set_policy('AUDIT_CNT', 'AUDIT_AHLT', 'AUDIT_ARGV', and/or 'AUDIT_ARGE')\n\
                                                                           \n\
                                                                           \n\
Set the audit policy flags. See auditon(2) for more information.");


PyDoc_STRVAR(au_get_kmask__doc__,
"au_get_kmask() -> {success:<unsigned int>, failure:<unsigned int>}        \n\
                                                                           \n\
                                                                           \n\
Get the kernel preselection masks. See auditon(2) for more information.");

PyDoc_STRVAR(au_set_kmask__doc__,
"au_set_kmask(success=<unsigned int>, failure=<unsigned int>)              \n\
                                                                           \n\
                                                                           \n\
Set the kernel preselection masks. See auditon(2) for more information.");

PyDoc_STRVAR(au_get_qctrl__doc__,
"au_get_qctrl() -> {hiwater:123, lowater:0, bufsz:, delay:, minfree:}      \n\
                                                                           \n\
                                                                           \n\
Get the kernel audit queue parameters. See auditon(2) for more information.");

PyDoc_STRVAR(au_set_qctrl__doc__,
"au_set_qctrl(hiwater=123, lowater=0)                                      \n\
                                                                           \n\
                                                                           \n\
Set the kernel audit queue parameters. See auditon(2) for more information.");

PyDoc_STRVAR(au_get_pinfo__doc__,
"au_get_pinfo(pid) -> {mask:{success:0, failure:0}, tid:{port:0 machine:1},\n\
			 asid:123}                                         \n\
                                                                           \n\
Get the audit settings for a process. See auditon(2) for more information.");

PyDoc_STRVAR(au_get_pinfo_addr__doc__,
"au_get_pinfo_addr(pid) -> {auid:5, mask:{success:0, failure:0},           \n\
        tid:{port:0 type:IPv4, addr:0.0.0.0}, asid:123}                    \n\
                                                                           \n\
                                                                           \n\
Get the extended audit settings for a process. See auditon(2) for more     \n\
information.");

PyDoc_STRVAR(au_get_fsize__doc__,
"au_get_fsize() -> {filesz:123, currsz:0}                                  \n\
                                                                           \n\
                                                                           \n\
Get the maximum size of the audit log file. See auditon(2) for more info.");

PyDoc_STRVAR(au_set_fsize__doc__,
"au_set_fsize(<unsigned long long>)                                        \n\
                                                                           \n\
                                                                           \n\
Set the maximum size of the audit log file. See auditon(2) for more info.");

PyDoc_STRVAR(au_getauid__doc__,
"au_getauid() -> <auid>                                                    \n\
                                                                           \n\
                                                                           \n\
Get the active audit session ID for current process. See getauid(2) for    \n\
more info.");

PyDoc_STRVAR(au_setauid__doc__,
"au_setauid(<auid>)                                                        \n\
                                                                           \n\
                                                                           \n\
Set the active audit session ID for current process. See setauid(2) for    \n\
more info.");


PyDoc_STRVAR(au_set_presel_masks__doc__,
"set_preselect_masks()                                                     \n\
                                                                           \n\
Set the event class preselection masks for all the audit events. This is   \n\
usually done by auditd(8) at start up. Therefore, this is for when auditd  \n\
is not being used.");

static PyObject *au_get_cond_py(PyObject *self, PyObject *args);
static PyObject *au_set_cond_py(PyObject *self, PyObject *args);
static PyObject *au_get_policy_py(PyObject *self, PyObject *args);
static PyObject *au_set_policy_py(PyObject *self, PyObject *args);
static PyObject *au_get_kmask_py(PyObject *self, PyObject *args);
static PyObject *au_set_kmask_py(PyObject *self, PyObject *args,
    PyObject *kwargs);
static PyObject *au_get_qctrl_py(PyObject *self, PyObject *args);
static PyObject *au_set_qctrl_py(PyObject *self, PyObject *args,
    PyObject *kwargs);
static PyObject *au_get_pinfo_py(PyObject *self, PyObject *args);
static PyObject *au_get_pinfo_addr_py(PyObject *self, PyObject *args);
static PyObject *au_get_fsize_py(PyObject *self, PyObject *args);
static PyObject *au_set_fsize_py(PyObject *self, PyObject *args);
static PyObject *au_getauid_py(PyObject *self, PyObject *args);
static PyObject *au_setauid_py(PyObject *self, PyObject *args);
static PyObject *au_set_presel_masks_py(PyObject *self, PyObject *args);

static PyMethodDef pybsmMethods[] = { 
	{"au_get_cond", au_get_cond_py, 0, au_get_cond__doc__},
	{"au_set_cond", au_set_cond_py, METH_VARARGS, au_set_cond__doc__},
	{"au_get_policy", au_get_policy_py, 0, au_get_policy__doc__},
	{"au_set_policy", au_set_policy_py, METH_VARARGS, au_set_policy__doc__},
	{"au_get_kmask", au_get_kmask_py, 0, au_get_kmask__doc__},
	{"au_set_kmask", (PyCFunction)au_set_kmask_py,
	    METH_VARARGS | METH_KEYWORDS, au_set_kmask__doc__},
	{"au_get_qctrl", au_get_qctrl_py, 0, au_get_qctrl__doc__},
	{"au_set_qctrl", (PyCFunction)au_set_qctrl_py,
	    METH_VARARGS | METH_KEYWORDS, au_set_qctrl__doc__},
	{"au_get_pinfo", au_get_pinfo_py, METH_VARARGS, au_get_pinfo__doc__},
	{"au_get_pinfo_addr", au_get_pinfo_addr_py, METH_VARARGS, 
	    au_get_pinfo_addr__doc__},
	{"au_get_fsize", au_get_fsize_py, 0, au_get_fsize__doc__},
	{"au_set_fsize", au_set_fsize_py, METH_VARARGS, au_set_fsize__doc__},
	{"au_getauid", au_getauid_py, 0, au_getauid__doc__},
	{"au_setauid", au_setauid_py, METH_VARARGS, au_setauid__doc__},
	{"au_set_presel_masks", au_set_presel_masks_py, 0, 
	    au_set_presel_masks__doc__},
	{NULL, NULL, 0, NULL} 
};

/*
 *  pybsm.io class methods.
 */
PyDoc_STRVAR(au_read_rec__doc__,
"o.au_read_rec() ->  ({tok0}, {tok1}, {tok2}... {tokN})                     \n\
                                                                            \n\
Returns a BSM record in a tuple containing token dictionaries. au_read_rec  \n\
is a method of the pybsm.io class. The BSM record typically contains a      \n\
header token, followed by zero or more varible argument token (path, ports, \n\
etc.), a subject token, a return token, and a trailer token.");

PyDoc_STRVAR(au_pipe_get_config__doc__,
"o.au_pipe_get_config() ->  {qlength:0, ...}                                \n\
                                                                            \n\
Returns all the auditpipe configuration parameters in a dictionary including\n\
qlength, qlimit, qlimit_min, qlimit_max, max_auditdata, preselect_mode      \n\
('local' or 'trail'), preselect_flags, and preselect_naflags.");

PyDoc_STRVAR(au_pipe_get_stats__doc__,
"o.au_pipe_get_stats() ->  {inserts:1234, ...}                              \n\
                                                                            \n\
Returns all the auditpipe statistics in a dictionary including inserts,     \n\
reads, drops, and truncates.");

PyDoc_STRVAR(au_pipe_set_config__doc__,
"o.set_auditpipe_parms(qlimit=256, ...)                                     \n\
                                                                            \n\
Sets the auditpipe configuraton parameters. The parameters may include:     \n\
qlength, qlimit, preselect_mode ('local' or 'trail'), preselect_flags, and  \n\
preselect_naflags.");

PyDoc_STRVAR(au_pipe_flush__doc__,
"o.auditpipe_flush()                                                        \n\
                                                                            \n\
Flush all outstanding records on the audit pipe; useful after setting       \n\
initialpreselection properties to delete records queued during the          \n\
configuration process which may not match the interests of the user         \n\
process.");

PyDoc_STRVAR(au_pipe_get_presel_auid__doc__,
"o.au_pipe_get_presel_auid(auid) ->  mask                                   \n\
                                                                            \n\
Return the preselection mask for a given audit ID.");

PyDoc_STRVAR(au_pipe_set_presel_auid__doc__,
"o.au_pipe_set_presel_auid(auid, mask)                                      \n\
                                                                            \n\
Set the current preselection masks for a specific auid on the pipe.");

PyDoc_STRVAR(au_pipe_del_presel_auid__doc__,
"o.au_pipe_del_presel_auid(auid)                                            \n\
                                                                            \n\
Delete the current preselection mask for a specific auid on the pipe.");

PyDoc_STRVAR(au_pipe_flush_presel_auid__doc__,
"o.flush_preselect_auid()                                                   \n\
                                                                            \n\
Delete all auid specific preselection specifications.");

static PyObject *io_init_py(PyObject *unself, PyObject* args);
static PyObject *au_read_rec_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_get_config_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_get_stats_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_set_config_py(PyObject *unself, PyObject *args,
    PyObject *kwargs);
static PyObject *au_pipe_flush_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_get_presel_auid_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_set_presel_auid_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_del_presel_auid_py(PyObject *unself, PyObject *args);
static PyObject *au_pipe_flush_presel_auid_py(PyObject *unself,
    PyObject *args);
static PyObject *io_destroy_py(PyObject *unself, PyObject* args);

static PyMethodDef ioMethods[] = {
	{"__init__", io_init_py, METH_VARARGS, "Init pybsm.io"},
	{"au_read_rec", au_read_rec_py, METH_VARARGS, au_read_rec__doc__},
	{"au_pipe_get_config", au_pipe_get_config_py, METH_VARARGS,
	     au_pipe_get_config__doc__},
	{"au_pipe_get_stats", au_pipe_get_stats_py, METH_VARARGS, 
	     au_pipe_get_stats__doc__},
	{"au_pipe_set_config", (PyCFunction)au_pipe_set_config_py,
	     METH_VARARGS | METH_KEYWORDS, au_pipe_set_config__doc__},
	{"au_pipe_flush", au_pipe_flush_py, METH_VARARGS, 
	     au_pipe_flush__doc__},
	{"au_pipe_get_presel_auid", au_pipe_get_presel_auid_py, METH_VARARGS,
	     au_pipe_get_presel_auid__doc__},
	{"au_pipe_set_presel_auid", au_pipe_set_presel_auid_py, METH_VARARGS,
	     au_pipe_set_presel_auid__doc__},
	{"au_pipe_del_presel_auid", au_pipe_del_presel_auid_py, METH_VARARGS,
	     au_pipe_del_presel_auid__doc__},
	{"au_pipe_flush_presel_auid", au_pipe_flush_presel_auid_py,
	     METH_VARARGS, au_pipe_flush_presel_auid__doc__},
	{"__del__", io_destroy_py, METH_O, "Destroy pybsm.io"},
	{NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initpybsm(void);

static PyObject *
pybsm_error(int errnum)
{
	PyObject *po;

	if (errnum == ENOMEM)
		return (PyErr_NoMemory());

	po = PyString_FromString(strerror(errnum));
	PyErr_SetObject(PyExc_RuntimeError, po);
	Py_DECREF(po);
	return (NULL);
}

static const char * 
ip_ex_address(char *ip_str, u_int32_t type, u_int32_t *ipaddr)
{
	struct in_addr ipv4;
	struct in6_addr ipv6;

	switch (type) {
	case AU_IPv4:
		ipv4.s_addr = (in_addr_t)(ipaddr[0]);
		return (inet_ntop(AF_INET, &ipv4, ip_str, 
			INET6_ADDRSTRLEN));
	case AU_IPv6:
		bcopy(ipaddr, &ipv6, sizeof(ipv6));
		return (inet_ntop(AF_INET6, &ipv6, ip_str,
			INET6_ADDRSTRLEN));
	default:
		return ("invalid");
	}	
}

static char *
ip_address(u_int32_t ip)
{
	struct in_addr ipaddr;

	ipaddr.s_addr = ip;
	return (inet_ntoa(ipaddr));
}

static PyObject *
c2py_header32(tokenstr_t *tok)
{
	char ev_name[AU_EVENT_NAME_MAX];
	char ev_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e;
	PyObject *po = NULL;

	bzero(&e, sizeof(e));
	bzero(ev_name, sizeof(ev_name));
	bzero(ev_desc, sizeof(ev_desc));
	e.ae_name = ev_name;
	e.ae_desc = ev_desc;

	if (getauevnum_r(&e, tok->tt.hdr32.e_type) == NULL)
		sprintf(ev_name, "%u", tok->tt.hdr32.e_type);
	po = Py_BuildValue(
		"{s:s, s:I, s:B, s:s, s:H, s:k, s:k}",
		"token", "header32",
		"size", tok->tt.hdr32.size,
		"version", tok->tt.hdr32.version,
		"event", ev_name,
		"modifier", tok->tt.hdr32.e_mod,
		"time", tok->tt.hdr32.s,
		"msec", tok->tt.hdr32.ms
		);
	return (po);
}

static PyObject *
c2py_header32_ex(tokenstr_t *tok)
{
	char ev_name[AU_EVENT_NAME_MAX];
	char ev_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e;
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;

	bzero(&e, sizeof(e));
	bzero(ev_name, sizeof(ev_name));
	bzero(ev_desc, sizeof(ev_desc));
	e.ae_name = ev_name;
	e.ae_desc = ev_desc;

	if (getauevnum_r(&e, tok->tt.hdr32_ex.e_type) == NULL)
		sprintf(ev_name, "%u", tok->tt.hdr32_ex.e_type);
	ex_ip_addr = ip_ex_address(ip_str, tok->tt.hdr32_ex.ad_type, 
	    tok->tt.hdr32_ex.addr);
	po = Py_BuildValue(
		"{s:s, s:I, s:B, s:s, s:H, s:s, s:k, s:k}",
		"token", "header32_ex",
		"size", tok->tt.hdr32.size,
		"version", tok->tt.hdr32_ex.version,
		"event", ev_name,
		"modifier", tok->tt.hdr32.e_mod,
		"ip", ex_ip_addr,
		"time", tok->tt.hdr32.s,
		"msec", tok->tt.hdr32.ms
		);
	return (po);
}

static PyObject *
c2py_header64(tokenstr_t *tok)
{
	char ev_name[AU_EVENT_NAME_MAX];
	char ev_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e;
	PyObject *po = NULL;

	bzero(&e, sizeof(e));
	bzero(ev_name, sizeof(ev_name));
	bzero(ev_desc, sizeof(ev_desc));
	e.ae_name = ev_name;
	e.ae_desc = ev_desc;

	if (getauevnum_r(&e, tok->tt.hdr64.e_type) == NULL)
		sprintf(ev_name, "%u", tok->tt.hdr64.e_type);
	po = Py_BuildValue(
		"{s:s, s:I, s:B, s:s, s:H, s:k, s:k}",
		"token", "header64",
		"size", tok->tt.hdr64.size,
		"version", tok->tt.hdr64.version,
		"event", ev_name,
		"modifier", tok->tt.hdr64.e_mod,
		"time", tok->tt.hdr64.s,
		"msec", tok->tt.hdr64.ms 
		);
	return (po);
}

static PyObject *
c2py_header64_ex(tokenstr_t *tok)
{
	char ev_name[AU_EVENT_NAME_MAX];
	char ev_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e;
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;

	bzero(&e, sizeof(e));
	bzero(ev_name, sizeof(ev_name));
	bzero(ev_desc, sizeof(ev_desc));
	e.ae_name = ev_name;
	e.ae_desc = ev_desc;

	if (getauevnum_r(&e, tok->tt.hdr64_ex.e_type) == NULL)
		sprintf(ev_name, "%u", tok->tt.hdr64_ex.e_type);
	ex_ip_addr = ip_ex_address(ip_str, 
	    tok->tt.hdr64_ex.ad_type, tok->tt.hdr64_ex.addr);
	po = Py_BuildValue(
		"{s:s, s:I, s:B, s:s, s:H, s:s, s:k, s:k}",
		"token", "header64_ex",
		"size", tok->tt.hdr64_ex.size,
		"version", tok->tt.hdr64_ex.version,
		"event", ev_name,
		"modifier", tok->tt.hdr64_ex.e_mod,
		"ip", ex_ip_addr,
		"time", tok->tt.hdr64_ex.s,
		"msec", tok->tt.hdr64_ex.ms 
		);
	return (po);
}

static PyObject *
c2py_trailer(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s, s:H, s:I}",
		"token", "trailer",
		"magic", tok->tt.trail.magic,
		"count", tok->tt.trail.count
		);
	return (po);
}

static PyObject *
c2py_arg32(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s, s:H, s:k, s:s#}",
		"token", "argument32",
		"arg-num", tok->tt.arg32.no,
		"value", tok->tt.arg32.val,
		"desc", tok->tt.arg32.text, 
		(tok->tt.arg32.len - 1)
		);
	return (po);
}

static PyObject *
c2py_arg64(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s, s:H, s:k, s:s#}",
		"token", "argument64",
		"arg-num", tok->tt.arg64.no,
		"value", tok->tt.arg64.val,
		"desc", tok->tt.arg64.text, 
		(tok->tt.arg64.len - 1)
		);
	return (po);
}

static PyObject *
c2py_data(tokenstr_t *tok)
{
	char *how;
	char *format;
	size_t size;
	PyObject *pso = NULL;
	PyObject *po = NULL;

	switch(tok->tt.arb.howtopr) {
	case AUP_BINARY:
		how = "binary";
		break;

	case AUP_OCTAL:
		how = "octal";
		break;

	case AUP_DECIMAL:
		how = "decimal";
		break;

	case AUP_HEX:
		how = "hex";
		break;

	case AUP_STRING:
		how = "string";
		break;

	default:
		return (NULL);
	}

	switch(tok->tt.arb.bu) {
	case AUR_BYTE:
		format = "b";
		size = AUR_BYTE_SIZE;
		break;

	case AUR_SHORT:
		format = "h";
		size = AUR_SHORT_SIZE;
		break;

	case AUR_INT32:
		format = "i";
		size = AUR_INT32_SIZE;
		break;

	case AUR_INT64:
		format = "l";
		size = AUR_INT64_SIZE;
	
	default:
		return (NULL);
	}

	/* Allocate string for 'pack'ed data */
	pso = PyString_FromStringAndSize((char *)NULL, size * tok->tt.arb.uc);
	if (pso == NULL)
		return (NULL);

	/* XXX may need to convert the data to python packed format */
	bcopy(tok->tt.arb.data, PyString_AS_STRING(pso), 
	    size * tok->tt.arb.uc);

	po = Py_BuildValue(
		"{s:s, s:I, s:s, s:s, s:B, s:O}",
		"token", "data",
		"bu_size", size,
		"bu_format", format,
		"bu_howtopr", how,
		"count", tok->tt.arb.uc,
		"data", pso
	);
	/* Py_DECREF(pso); */
	return (po);
}

static PyObject *
c2py_attr32(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:k s:I}",
		"token", "attribute32",
		"mode",	tok->tt.attr32.mode,
		"uid", tok->tt.attr32.uid,
		"gid", tok->tt.attr32.gid,
		"fsid", tok->tt.attr32.fsid,
		"nodeid", tok->tt.attr32.nid,
		"device", tok->tt.attr32.dev
		);
	return (po);
}

static PyObject *
c2py_attr64(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:k s:k}",
		"token", "attribute64",
		"mode",	tok->tt.attr64.mode,
		"uid", tok->tt.attr64.uid,
		"gid", tok->tt.attr64.gid,
		"fsid", tok->tt.attr64.fsid,
		"nodeid", tok->tt.attr64.nid,
		"device", tok->tt.attr64.dev
		);     
	return (po);
}

static PyObject *
c2py_exit(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I}",
		"token", "exit",
		"errval", tok->tt.exit.status,
		"retval", tok->tt.exit.ret
		);
	return (po);
}

static PyObject *
c2py_exec_args(tokenstr_t *tok)
{
	Py_ssize_t i;
	PyObject *po = NULL;
	PyObject *so = NULL;
	PyObject *to = NULL;

	to = PyTuple_New(tok->tt.execarg.count);
	for (i = 0; (unsigned)i < tok->tt.execarg.count; i++) {
		so = PyString_FromString(tok->tt.execarg.text[i]);
		PyTuple_SetItem(to, i, so);
		/* Py_DECREF(so); */
	}
	po = Py_BuildValue(
		"{s:s s:I s:O}",
		"token", "exec_args",
		"count", tok->tt.execarg.count,
		"arg", to	
		);
	/* Py_DECREF(to); */
	return (po);
}

static PyObject *
c2py_exec_env(tokenstr_t *tok)
{
	Py_ssize_t i;
	PyObject *po = NULL;
	PyObject *so = NULL;
	PyObject *to = NULL;

	to = PyTuple_New(tok->tt.execenv.count);
	for (i = 0; (unsigned)i < tok->tt.execenv.count; i++) {
		so = PyString_FromString(tok->tt.execenv.text[i]);
		PyTuple_SetItem(to, i, so);
		/* Py_DECREF(so); */
	}
	po = Py_BuildValue(
		"{s:s s:I s:O}",
		"token", "exec_env",
		"count", tok->tt.execenv.count,
		"env", to
		);
	/* Py_DECREF(to); */
	return (po);
}

static PyObject *
c2py_other_file32(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:s#}",
		"token", "file32",
		"time", tok->tt.file.s,
		"msec", tok->tt.file.ms,
		"name", tok->tt.file.name, (tok->tt.file.len - 1)
		);
	return (po);
}

static PyObject *
c2py_newgroups(tokenstr_t *tok)
{
	Py_ssize_t i;
	PyObject *po = NULL;
	PyObject *to = NULL;
	PyObject *io = NULL;

	to = PyTuple_New(tok->tt.grps.no);
	for (i = 0; i < tok->tt.grps.no; i++) {
		io = PyInt_FromLong((long) tok->tt.grps.list[i]);
		PyTuple_SetItem(to, i, io);
		/* Py_DECREF(io); */
	}
	po = Py_BuildValue(
		"{s:s s:H s:O}",
		"token", "group",
		"count", tok->tt.grps.no,
		"gid", to
		);
	/* Py_DECREF(to); */
	return (po);
}

static PyObject *
c2py_in_addr(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:s}",
		"token", "in_addr",
		"addr", ip_address(tok->tt.inaddr.addr)
		);
	return (po);
}

static PyObject *
c2py_in_addr_ex(tokenstr_t *tok)
{
	PyObject *po = NULL;
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;

	ex_ip_addr = ip_ex_address(ip_str, tok->tt.inaddr_ex.type, 
	    tok->tt.inaddr_ex.addr);
	po = Py_BuildValue(
		"{s:s s:s}",
		"token", "in_addr_ex",
		"ip_addr_ex", ex_ip_addr
		);
	return (po);
}

static PyObject *
c2py_ip(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:B s:B s:H s:H s:H s:B s:B s:H s:s s:s}",
		"token", "ip",
		"version", tok->tt.ip.version,
		"service_type", tok->tt.ip.tos, 
		"len", tok->tt.ip.len,
		"id", tok->tt.ip.id,
		"offset", tok->tt.ip.offset,
		"time_to_live", tok->tt.ip.ttl,
		"protocol", tok->tt.ip.prot,
		"cksum", tok->tt.ip.chksm,
		"src_addr", ip_address(tok->tt.ip.src),
		"dest_addr", ip_address(tok->tt.ip.dest)
		);
	return (po);
}

static PyObject *
c2py_ipc(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:B s:I}",
		"token", "IPC",
		"ipc-type", tok->tt.ipc.type,
		"ipc-id", tok->tt.ipc.id
		);
	return (po);
}

static PyObject *
c2py_ipc_perm(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I}",
		"token", "ipc_perm",
		"uid", tok->tt.ipcperm.uid,
		"gid", tok->tt.ipcperm.gid,
		"creator-uid", tok->tt.ipcperm.puid,
		"creator-gid", tok->tt.ipcperm.pgid,
		"mode", tok->tt.ipcperm.mode,
		"seq", tok->tt.ipcperm.seq,
		"key", tok->tt.ipcperm.key
		);
	return (po);
}

static PyObject *
c2py_iport(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:I}",
		"token", "ip_port",
		"port", tok->tt.iport.port
		);
	return (po);
}

static PyObject *
c2py_opaque(tokenstr_t *tok)
{
	PyObject *po = NULL;
	PyObject *so = NULL;

	/* Allocate string for 'pack'ed data */
	so = PyString_FromStringAndSize((char *)NULL, tok->tt.opaque.size);
	if (so == NULL)
		return (NULL);
	/* XXX may need to convert the data to python packed format. */
	bcopy(tok->tt.opaque.data, PyString_AS_STRING(so), 
	    tok->tt.opaque.size);
	po = Py_BuildValue(
		"{s:s, s:H, s:O}",
		"token", "opaque",
		"size", tok->tt.opaque.size,
		"data", so
		);
	/* Py_DECREF(so); */
	return (po);
}

static PyObject *
c2py_path(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:s#}",
		"token", "path",
		"path", tok->tt.path.path, (tok->tt.path.len - 1)
		);
	return (po);
}

static PyObject *
c2py_process32(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:I s:s}",
		"token", "process32",
		"audit-uid", tok->tt.proc32.auid,
		"uid", tok->tt.proc32.euid,
		"gid", tok->tt.proc32.egid,
		"ruid", tok->tt.proc32.ruid,
		"rgid", tok->tt.proc32.rgid,
		"pid", tok->tt.proc32.pid,
		"sid", tok->tt.proc32.sid,
		"tid_port", tok->tt.proc32.tid.port,
		"tid_addr", ip_address(tok->tt.proc32.tid.addr)
		);
	return (po);
}

static PyObject *
c2py_process32_ex(tokenstr_t *tok)
{
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;

	ex_ip_addr = ip_ex_address(ip_str, 
	    tok->tt.proc32_ex.tid.type, 
	    tok->tt.proc32_ex.tid.addr);
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:I s:s}",
		"token", "process32_ex",
		"audit-uid", tok->tt.proc32_ex.auid,
		"uid", tok->tt.proc32_ex.euid,
		"gid", tok->tt.proc32_ex.egid,
		"ruid", tok->tt.proc32_ex.ruid,
		"rgid", tok->tt.proc32_ex.rgid,
		"pid", tok->tt.proc32_ex.pid,
		"sid", tok->tt.proc32_ex.sid,
		"tid_port", tok->tt.proc32_ex.tid.port,
		"tid_addr", ex_ip_addr
		);
	return (po);
}

static PyObject *
c2py_process64(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:k s:s}",
		"token", "process64",
		"audit-uid", tok->tt.proc64.auid,
		"uid", tok->tt.proc64.euid,
		"gid", tok->tt.proc64.egid,
		"ruid", tok->tt.proc64.ruid,
		"rgid", tok->tt.proc64.rgid,
		"pid", tok->tt.proc64.pid,
		"sid", tok->tt.proc64.sid,
		"tid_port", tok->tt.proc64.tid.port,
		"tid_addr", ip_address(tok->tt.proc64.tid.addr)
		);
	return (po);
}

static PyObject *
c2py_process64_ex(tokenstr_t *tok)
{
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;
	
	ex_ip_addr = ip_ex_address(ip_str, 
	    tok->tt.proc64_ex.tid.type, 
	    tok->tt.proc64_ex.tid.addr);
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:k s:s}",
		"token", "process64_ex",
		"audit-uid", tok->tt.proc64_ex.auid,
		"uid", tok->tt.proc64_ex.euid,
		"gid", tok->tt.proc64_ex.egid,
		"ruid", tok->tt.proc64_ex.ruid,
		"rgid", tok->tt.proc64_ex.rgid,
		"pid", tok->tt.proc64_ex.pid,
		"sid", tok->tt.proc64_ex.sid,
		"tid_port", tok->tt.proc64_ex.tid.port,
		"tid_addr", ex_ip_addr
		);
	return (po);
}

static PyObject *
c2py_return32(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:B s:I}",
		"token", "return32",
		"errval", tok->tt.ret32.status,
		"retval", tok->tt.ret32.ret		
		);
	return (po);
}

static PyObject *
c2py_return64(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:B s:k}",
		"token", "return64",
		"errval", tok->tt.ret32.status,
		"retval", tok->tt.ret32.ret		
		);
	return (po);
}

static PyObject *
c2py_seq(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:I}",
		"token", "seq",
		"seq-num", tok->tt.seq.seqno
		);
	return (po);
}

static PyObject *
c2py_socket(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:H s:H s:s s:H s:s}",
		"token", "socket",
		"sock_type", tok->tt.socket.type,
		"lport", ntohs(tok->tt.socket.l_port),
		"laddr", ip_address(tok->tt.socket.l_addr),
		"fport", ntohs(tok->tt.socket.r_port),
		"faddr", ip_address(tok->tt.socket.r_addr)
		);
	return (po);
}

static PyObject *
c2py_sockinet32(tokenstr_t *tok)
{
	PyObject *po = NULL;
	
	po = Py_BuildValue(
		"{s:s s:H s:H s:s}",
		"token", "sockinet32",
		"type", tok->tt.sockinet32.family,
		"port", tok->tt.sockinet32.port,
		"addr", ip_address(tok->tt.sockinet32.addr)
		);
	return (po);
}

static PyObject *
c2py_sockunix(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:H s:s}",
		"token", "sockunix",
		"type", tok->tt.sockunix.family,
		"port", 0,
		"addr", tok->tt.sockunix.path
		);
	return (po);
}

static PyObject *
c2py_subject32(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:I s:s}",
		"token", "subject32",
		"audit-id", tok->tt.subj32.auid,
		"uid", tok->tt.subj32.euid,
		"gid", tok->tt.subj32.egid,
		"ruid", tok->tt.subj32.ruid,
		"rgid", tok->tt.subj32.rgid,
		"pid", tok->tt.subj32.pid,
		"sid", tok->tt.subj32.sid,
		"tid_port", tok->tt.subj32.tid.port,
		"tid_addr", ip_address(tok->tt.subj32.tid.addr)
		);
	return (po);
}

static PyObject *
c2py_subject64(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:k s:s}",
		"token", "subject64",
		"audit-id", tok->tt.subj64.auid,
		"uid", tok->tt.subj64.euid,
		"gid", tok->tt.subj64.egid,
		"ruid", tok->tt.subj64.ruid,
		"rgid", tok->tt.subj64.rgid,
		"pid", tok->tt.subj64.pid,
		"sid", tok->tt.subj64.sid,
		"tid_port", tok->tt.subj64.tid.port,
		"tid_addr", ip_address(tok->tt.subj64.tid.addr)
		);
	return (po);
}

static PyObject *
c2py_subject32_ex(tokenstr_t *tok)
{
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;
	
	ex_ip_addr = ip_ex_address(ip_str, 
	    tok->tt.subj32_ex.tid.type, 
	    tok->tt.subj32_ex.tid.addr);
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:I s:s}",
		"token", "subject32_ex",
		"audit-id", tok->tt.subj32_ex.auid,
		"uid", tok->tt.subj32_ex.euid,
		"gid", tok->tt.subj32_ex.egid,
		"ruid", tok->tt.subj32_ex.ruid,
		"rgid", tok->tt.subj32_ex.rgid,
		"pid", tok->tt.subj32_ex.pid,
		"sid", tok->tt.subj32_ex.sid,
		"tid_port", tok->tt.subj32_ex.tid.port,
		"tid_addr", ex_ip_addr
		);
	return (po);
}

static PyObject *
c2py_subject64_ex(tokenstr_t *tok)
{
	char ip_str[INET6_ADDRSTRLEN];
	const char *ex_ip_addr;
	PyObject *po = NULL;
	
	ex_ip_addr = ip_ex_address(ip_str, 
	    tok->tt.subj64_ex.tid.type, 
	    tok->tt.subj64_ex.tid.addr);
	po = Py_BuildValue(
		"{s:s s:I s:I s:I s:I s:I s:I s:I s:k s:s}",
		"token", "subject64_ex",
		"audit-id", tok->tt.subj64_ex.auid,
		"uid", tok->tt.subj64_ex.euid,
		"gid", tok->tt.subj64_ex.egid,
		"ruid", tok->tt.subj64_ex.ruid,
		"rgid", tok->tt.subj64_ex.rgid,
		"pid", tok->tt.subj64_ex.pid,
		"sid", tok->tt.subj64_ex.sid,
		"tid_port", tok->tt.subj64_ex.tid.port,
		"tid_addr", ex_ip_addr 
		);
	return (po);
}

static PyObject *
c2py_text(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:s#}",
		"token", "text",
		"text", tok->tt.text.text, (tok->tt.text.len - 1)
		);
	return (po);
}

static PyObject *
c2py_socket_ex(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:H s:H s:H}",
		"token", "socket_ex",
		"sock_type", tok->tt.socket_ex32.type,
		"lport", ntohs(tok->tt.socket_ex32.l_port),
		"laddr", ip_address(tok->tt.socket_ex32.l_addr),
		"fport", ntohs(tok->tt.socket_ex32.r_port),
		"faddr", ip_address(tok->tt.socket.r_addr)
		);
	return (po);
}

static PyObject *
c2py_zonename(tokenstr_t *tok)
{
	PyObject *po = NULL;

	po = Py_BuildValue(
		"{s:s s:s}",
		"token", "zonename",
		"zone", tok->tt.zonename.zonename, (tok->tt.zonename.len - 1)
		);
	return (po);
}

static PyObject *
au_read_rec_py(__unused PyObject *unself, PyObject *args)
{
	tokenstr_t tok;
	PyObject *po = NULL;
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;
	int err, bytesread, reclen;
	struct token_py *tp;
	struct token_head_py tp_head = STAILQ_HEAD_INITIALIZER(tp_head);
	Py_ssize_t tcnt, li;
	u_char *rbuf;

	STAILQ_INIT(&tp_head);

	if (!PyArg_ParseTuple(args, "O:au_read_rec", &self)) 
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);
	Py_BEGIN_ALLOW_THREADS
	reclen = au_read_rec(ctx->io_fp, &rbuf);
	Py_END_ALLOW_THREADS
	if (reclen == -1) 
		return (pybsm_error(errno));
	bytesread = tcnt = 0;
	while (bytesread < reclen) {
		err = au_fetch_tok(&tok, rbuf + bytesread, reclen - bytesread);
		if (err) {
			free(rbuf);
			po = PyString_FromString("Invalid audit record");
			PyErr_SetObject(PyExc_RuntimeError, po);
			Py_DECREF(po);
			return (NULL);
		}
		bytesread += tok.len;

		switch(tok.id) {
		case AUT_HEADER32:
			po = c2py_header32(&tok);
			break;
	
		case AUT_HEADER32_EX:
			po = c2py_header32_ex(&tok);
			break;

		case AUT_HEADER64:
			po = c2py_header64(&tok);
			break;

		case AUT_HEADER64_EX:
			po = c2py_header64_ex(&tok);
			break;

		case AUT_TRAILER:
			po = c2py_trailer(&tok);
			break;

		case AUT_ARG32:
			po = c2py_arg32(&tok);
			break;

		case AUT_ARG64:
			po = c2py_arg64(&tok);
			break;

		case AUT_DATA:
			po = c2py_data(&tok);
			break;

		case AUT_ATTR32:
			po = c2py_attr32(&tok);
			break;

		case AUT_ATTR64:
			po = c2py_attr64(&tok);
			break;
	
		case AUT_EXIT:
			po = c2py_exit(&tok);
			break;

		case AUT_EXEC_ARGS:
			po = c2py_exec_args(&tok);
			break;

		case AUT_EXEC_ENV:
			po = c2py_exec_env(&tok);
			break;

		case AUT_OTHER_FILE32:
			po = c2py_other_file32(&tok);
			break;

		case AUT_NEWGROUPS:
			po = c2py_newgroups(&tok);
			break;
	
		case AUT_IN_ADDR:
			po = c2py_in_addr(&tok);
			break;

		case AUT_IN_ADDR_EX:
			po = c2py_in_addr_ex(&tok);
			break;

		case AUT_IP:
			po = c2py_ip(&tok);
			break;

		case AUT_IPC:
			po = c2py_ipc(&tok);
			break;

		case AUT_IPC_PERM:
			po = c2py_ipc_perm(&tok);
			break;

		case AUT_IPORT:
			po = c2py_iport(&tok);
			break;

		case AUT_OPAQUE:
			po = c2py_opaque(&tok);
			break;

		case AUT_PATH:
			po = c2py_path(&tok);
			break;

		case AUT_PROCESS32:
			po = c2py_process32(&tok);
			break;

		case AUT_PROCESS32_EX:
			po = c2py_process32_ex(&tok);
			break;

		case AUT_PROCESS64:
			po = c2py_process64(&tok);
			break;

		case AUT_PROCESS64_EX:
			po = c2py_process64_ex(&tok);
			break;

		case AUT_RETURN32:
			po = c2py_return32(&tok);
			break;

		case AUT_RETURN64:
			po = c2py_return64(&tok);
			break;

		case AUT_SEQ:
			po = c2py_seq(&tok);
			break;
	
		case AUT_SOCKET:
			po = c2py_socket(&tok);
			break;

		case AUT_SOCKINET32:
			po = c2py_sockinet32(&tok);
			break;

		case AUT_SOCKUNIX:
			po = c2py_sockunix(&tok);
			break;

		case AUT_SUBJECT32:
			po = c2py_subject32(&tok);
			break;

		case AUT_SUBJECT64:
			po = c2py_subject64(&tok);
			break;

		case AUT_SUBJECT32_EX:
			po = c2py_subject32_ex(&tok);
			break;

		case AUT_SUBJECT64_EX:
			po = c2py_subject64_ex(&tok);
			break;

		case AUT_TEXT:
			po = c2py_text(&tok);
			break;
	
		case AUT_SOCKET_EX:
			po = c2py_socket_ex(&tok);
			break;

		case AUT_ZONENAME:
			po = c2py_zonename(&tok);
			break;

		default:
			po = Py_BuildValue("{s:s, s:I}","token", "unknown",
			    "id", tok.id);
			break;
	
		}
		if (po == NULL) {
			free(rbuf);
			return (pybsm_error(ENOMEM));
		}
		if ((tp = malloc(sizeof(struct token_py))) == NULL) {
			free(rbuf);
			return (pybsm_error(ENOMEM));
		}
		tp->t_po = po;
		STAILQ_INSERT_TAIL(&tp_head, tp, tokens_py);
		tcnt++;
	}

	li = 0;
	po = PyTuple_New(tcnt); 
	while (!STAILQ_EMPTY(&tp_head)) {
		tp = STAILQ_FIRST(&tp_head);
		PyTuple_SetItem(po, li++, tp->t_po);
		/* Py_DECREF(tp->t_po); */
		STAILQ_REMOVE_HEAD(&tp_head, tokens_py);
		free(tp);
	}
	free(rbuf);
	Py_INCREF(po);
	return (po);

}

static PyObject *
au_set_presel_masks_py(__unused PyObject *self, __unused PyObject *args)
{
	char ev_name[AU_EVENT_NAME_MAX];
	char ev_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e, *ep;
	struct au_evclass_map evc_map;

	e.ae_name = ev_name;
	e.ae_desc = ev_desc;

	setauevent();
	while((ep = getauevent_r(&e)) != NULL) {
		evc_map.ec_number = ep->ae_number;
		evc_map.ec_class = ep->ae_class;
		if (auditon(A_SETCLASS, &evc_map, sizeof(evc_map)) != 0)
			return (pybsm_error(errno));
	}
	endauevent();
	
	Py_RETURN_NONE;
}

static PyObject *
au_get_cond_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	u_int	cond;
	
	if (auditon(A_GETCOND, &cond, sizeof(cond)) != 0)
		return (pybsm_error(errno));
	
	switch (cond) {
	case AUC_AUDITING:
		po = PyString_FromString("AUC_AUDITING");
		break;
	
	case AUC_NOAUDIT:
		po = PyString_FromString("AUC_NOAUDIT");
		break;

	case AUC_DISABLED:
		po = PyString_FromString("AUC_DISABLED");
		break;
	
	default:
		po = PyString_FromString("UNKNOWN");
		break;
	}	
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_set_cond_py(__unused PyObject *self, PyObject *args)
{
	const char *p;	
	u_int	cond = 0;
	
	if (!PyArg_ParseTuple(args, "s:au_set_cond", &p))
		return (NULL);

	if (!strcmp(p, "AUC_AUDITING"))
		cond = AUC_AUDITING;
	else if (!strcmp(p, "AUC_NOAUDIT"))
		cond = AUC_NOAUDIT; 
	else if (!strcmp(p, "AUC_DISABLED"))
		cond = AUC_DISABLED; 
	else 
		return (pybsm_error(EINVAL));

	if (auditon(A_SETCOND, &cond, sizeof(cond)) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_get_policy_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	PyObject *so = NULL;
	u_int	pol, v;
	Py_ssize_t c;
	
	if (auditon(A_GETPOLICY, &pol, sizeof(pol)) != 0)
		return (pybsm_error(errno));

	/*
	 * quick count of bits set.
	 */
	for (v = pol, c = 0; v; c++)
		v &= v - 1;

	po = PyTuple_New(c);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	c = 0;
	if (pol & AUDIT_CNT) {
		so = PyString_FromString("AUDIT_CNT");
		if (so == NULL)
			goto enomem;
		PyTuple_SET_ITEM(po, c++, so);
	}
	if (pol & AUDIT_AHLT) {
		so = PyString_FromString("AUDIT_AHLT");
		if (so == NULL)
			goto enomem;
		PyTuple_SET_ITEM(po, c++, so);
	}
	if (pol & AUDIT_ARGV) {
		so = PyString_FromString("AUDIT_ARGV");
		if (so == NULL)
			goto enomem;
		PyTuple_SET_ITEM(po, c++, so);
	}
	if (pol & AUDIT_ARGE) {
		so = PyString_FromString("AUDIT_ARGE");
		if (so == NULL)
			goto enomem;
		PyTuple_SET_ITEM(po, c++, so);
	}
	Py_INCREF(po);
	return (po);

enomem:
	Py_DECREF(po);
	return (pybsm_error(ENOMEM));
}

static PyObject *
au_set_policy_py(__unused PyObject *self, PyObject *args)
{
	Py_ssize_t na, i;
	PyObject *so;
	char *str;
	long pol = 0;
	

	na = PyTuple_Size(args);
	for(i = 0; i < na; i++) {
		so = PyTuple_GetItem(args, i);		
		if (so == NULL)
			return (NULL);
		if (!PyString_Check(so))
			return (pybsm_error(EINVAL));
		str = PyString_AsString(so);
		if (!strcmp(str, "AUDIT_CNT")) 
			pol |= AUDIT_CNT;
		else if (!strcmp(str, "AUDIT_AHLT"))
			pol |= AUDIT_AHLT;
		else if (!strcmp(str, "AUDIT_ARGV"))
			pol |= AUDIT_ARGV;
		else if (!strcmp(str, "AUDIT_ARGE"))
			pol |= AUDIT_ARGE;
		else
			return (pybsm_error(EINVAL));
	}
	if (auditon(A_SETPOLICY, &pol, sizeof(pol)) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}
	
static PyObject *
au_get_kmask_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	au_mask_t am;

	if (auditon(A_GETKMASK, &am, sizeof(am)) != 0)
		return (pybsm_error(errno));
	po = Py_BuildValue("{s:I, s:I}",
	    "success", am.am_success,
	    "failure", am.am_failure);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_set_kmask_py(__unused PyObject *self, PyObject *args, PyObject *kwargs)
{
	au_mask_t am;
	static char *argnames[] = {"success", "failure", NULL};

	if (auditon(A_GETKMASK, &am, sizeof(am)) != 0)
		return (pybsm_error(errno));
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, 
	    "|II:au_set_kmask", argnames, &(am.am_success), &(am.am_failure)))
		return (NULL);
	if (auditon(A_SETKMASK, &am, sizeof(am)) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_get_fsize_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	au_fstat_t afs;

	if (auditon(A_GETFSIZE, &afs, sizeof(afs)) != 0)
		return (pybsm_error(errno));
	po = Py_BuildValue("{s:K, s:K}",
	    "filesz", afs.af_filesz,
	    "currsz", afs.af_currsz);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_set_fsize_py(__unused PyObject *self, PyObject *args)
{
	au_fstat_t afs;
	
	if (!PyArg_ParseTuple(args, "K:au_set_fsize", &(afs.af_filesz)))
		return (NULL);
	if (auditon(A_SETFSIZE, &afs, sizeof(afs)) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_getauid_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	au_id_t auid;

	if (getauid(&auid) != 0)
		return (pybsm_error(errno));
	po = Py_BuildValue("I", auid);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_setauid_py(__unused PyObject *self, PyObject *args)
{
	au_id_t auid;
	
	if (!PyArg_ParseTuple(args, "I:au_setauid", &auid))
		return (NULL);
	if (setauid(&auid) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_get_qctrl_py(__unused PyObject *self, __unused PyObject *args)
{
	PyObject *po = NULL;
	au_qctrl_t aqc;

	if (auditon(A_GETQCTRL, &aqc, sizeof(aqc)) != 0)
		return (pybsm_error(errno));
	po = Py_BuildValue("{s:k, s:k, s:k, s:k, s:i}",
	    "hiwater", aqc.aq_hiwater,
	    "lowater", aqc.aq_lowater,
	    "bufsz", aqc.aq_bufsz,
	    "delay", aqc.aq_delay,
	    "minfree", aqc.aq_minfree);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_set_qctrl_py(__unused PyObject *self, PyObject *args, PyObject *kwargs)
{
	au_qctrl_t aqc;
	static char *argnames[] = {"hiwater", "lowater", "bufsz", "delay",
	    "minfree", NULL};

	if (auditon(A_GETQCTRL, &aqc, sizeof(aqc)) != 0)
		return (pybsm_error(errno));
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, 
	    "|kkkki:au_set_qctrl", argnames, &(aqc.aq_hiwater),
	    &(aqc.aq_lowater), &(aqc.aq_bufsz), &(aqc.aq_delay),
	    &(aqc.aq_minfree)))
		return (NULL);
	if (auditon(A_SETQCTRL, &aqc, sizeof(aqc)) != 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_get_pinfo_py(__unused PyObject *self, PyObject *args)
{
	PyObject *po = NULL;
	PyObject *mo = NULL;
	PyObject *to = NULL;
	auditpinfo_t api;

	if (!PyArg_ParseTuple(args, "I:au_get_pinfo", &(api.ap_pid)))
		return (NULL);
	if (auditon(A_GETPINFO, &api, sizeof(api)) != 0)
		return (pybsm_error(errno));
	mo = Py_BuildValue("{s:I, s:I}",
	    "success", api.ap_mask.am_success,
	    "failure", api.ap_mask.am_failure);
	to = Py_BuildValue("{s:i, s:I}",
	    "port", api.ap_termid.port,
	    "machine", api.ap_termid.machine);
	po = Py_BuildValue("{s:I, s:O, s:O, s:I}",
	    "auid", api.ap_auid,
	    "mask", mo,
	    "termid", to,
	    "asid", api.ap_asid);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_get_pinfo_addr_py(__unused PyObject *self, PyObject *args)
{
	PyObject *po = NULL;
	PyObject *mo = NULL;
	PyObject *to = NULL;
	auditpinfo_addr_t api;

	if (!PyArg_ParseTuple(args, "i:au_get_pinfo_addr", &(api.ap_pid)))
		return (NULL);
	if (auditon(A_GETPINFO_ADDR, &api, sizeof(api)) != 0)
		return (pybsm_error(errno));
	mo = Py_BuildValue("{s:I, s:I}",
	    "success", api.ap_mask.am_success,
	    "failure", api.ap_mask.am_failure);
	/* XXX needs to be fixed to include addr, etc. */
	to = Py_BuildValue("{s:i, s:I}",
	    "port", api.ap_termid.at_port,
	    "type", api.ap_termid.at_type
	    );
	po = Py_BuildValue("{s:I, s:O, s:O, s:I}",
	    "auid", api.ap_auid,
	    "mask", mo,
	    "termid", to,
	    "asid", api.ap_asid);
	if (po == NULL)
		return (pybsm_error(ENOMEM));
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_pipe_get_config_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *po = NULL;
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;
	u_int qlen, qlim, qlim_min, qlim_max;
	u_int admax, pflgs, pnaflgs;
	int pmode;

	if (!PyArg_ParseTuple(args, "O:au_pipe_get_config", &self))
		return (NULL);

	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_GET_QLEN, &qlen) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_QLIMIT, &qlim) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_QLIMIT_MIN, &qlim_min) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_QLIMIT_MAX, &qlim_max) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_MAXAUDITDATA, &admax) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_MODE, &pmode) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_FLAGS, &pflgs) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_NAFLAGS, &pnaflgs) < 0)
		return (pybsm_error(errno));
	po = Py_BuildValue(
	    "{s:I, s:I, s:I, s:I, s:I, s:s, s:I, s:I}",
		"qlength", qlen,
		"qlimit", qlim,
		"qlimit_min", qlim_min,
		"qlimit_max", qlim_max,
		"max_auditdata", admax,
		"preselect_mode", (pmode == AUDITPIPE_PRESELECT_MODE_TRAIL) ?
		    "trail" : "local",
		"preselect_flags", pflgs,
		"preselect_naflags", pnaflgs
	);
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_pipe_get_stats_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *po = NULL;
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;
	u_int inserts, reads, drops, truncs;

	if (!PyArg_ParseTuple(args, "O:au_pipe_get_stats", &self))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_GET_INSERTS, &inserts) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_READS, &reads) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_DROPS, &drops) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_TRUNCATES, &truncs) < 0)
		return (pybsm_error(errno));
	po = Py_BuildValue(
	    "{s:I, s:I, s:I, s:I}",
		"inserts", inserts,
		"reads", reads,
		"drops", drops,
		"truncates", truncs
	);
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_pipe_set_config_py(__unused PyObject *unself, PyObject *args,
    PyObject *kwargs)
{
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;
	u_int qlim, pflgs, pnaflgs;
	int pmode;
	char *mode;
	static char *argnames[] = {"self", "qlimit", "preselect_flags", 
	    "preselect_naflags", "preselect_mode", NULL};

	/*
	 * Get file descriptor and read the current state of the parameters.
	 */
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, 
	    "O|IIIs:au_pipe_set_config", argnames, &self, &qlim, &pflgs,
	     &pnaflgs, &mode)) 
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_GET_QLIMIT, &qlim) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_FLAGS, &pflgs) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_NAFLAGS, &pnaflgs) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_MODE, &pmode) < 0)
		return (pybsm_error(errno));
	mode = (pmode == AUDITPIPE_PRESELECT_MODE_TRAIL) ? "trail" : "local"; 

	if (!PyArg_ParseTupleAndKeywords(args, kwargs,
	    "O|IIIs:au_pipe_set_config", argnames, &self, &qlim, &pflgs,
	    &pnaflgs, &mode))
		return (NULL);
	if (ioctl(ctx->io_fd, AUDITPIPE_SET_QLIMIT, &qlim) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_SET_PRESELECT_FLAGS, &pflgs) < 0)
		return (pybsm_error(errno));
	if (ioctl(ctx->io_fd, AUDITPIPE_SET_PRESELECT_NAFLAGS, &pnaflgs) < 0)
		return (pybsm_error(errno));
	if (strncmp("trail", mode, 5) == 0) 
		pmode = AUDITPIPE_PRESELECT_MODE_TRAIL;
	else
		pmode = AUDITPIPE_PRESELECT_MODE_LOCAL; 
	if (ioctl(ctx->io_fd, AUDITPIPE_SET_PRESELECT_MODE, &pmode) < 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_pipe_flush_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;

	if (!PyArg_ParseTuple(args, "O:au_pipe_flush", &self))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_FLUSH) < 0)
		return (pybsm_error(errno));
	Py_RETURN_NONE;
}

static PyObject *
au_pipe_get_presel_auid_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *self = NULL;
	PyObject *po = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;
	struct auditpipe_ioctl_preselect aip;

	if (!PyArg_ParseTuple(args, "OI:au_pipe_get_presel_auid", &self,
	    &(aip.aip_auid)))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_GET_PRESELECT_AUID, &aip) < 0)
		return (pybsm_error(errno));

	po = Py_BuildValue("I", aip.aip_mask);
	Py_INCREF(po);
	return (po);
}

static PyObject *
au_pipe_set_presel_auid_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *cobj = NULL;
	PyObject *self = NULL;
	struct io_ctx *ctx;
	struct auditpipe_ioctl_preselect aip;

	if (!PyArg_ParseTuple(args, "OII:au_pipe_set_presel_auid", &self,
	    &(aip.aip_auid), &(aip.aip_mask)))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_SET_PRESELECT_AUID, &aip) < 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_pipe_del_presel_auid_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *cobj = NULL;
	PyObject *self = NULL;
	struct io_ctx *ctx;
	au_id_t auid;;

	if (!PyArg_ParseTuple(args, "OI:au_pipe_del_presel_auid", &self,
	    &auid))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_DELETE_PRESELECT_AUID, &auid) < 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static PyObject *
au_pipe_flush_presel_auid_py(__unused PyObject *unself, PyObject *args)
{
	PyObject *cobj = NULL;
	PyObject *self = NULL;
	struct io_ctx *ctx;

	if (!PyArg_ParseTuple(args, "O:au_pipe_flush_presel_auid", &self))
		return (NULL);
	cobj = PyObject_GetAttrString(self, "_ctx");
	ctx  = PyCObject_AsVoidPtr(cobj);

	if (ioctl(ctx->io_fd, AUDITPIPE_FLUSH_PRESELECT_AUID) < 0)
		return (pybsm_error(errno));

	Py_RETURN_NONE;
}

static struct io_ctx *
init_ctx(char *fn)
{
	struct io_ctx *ctx;
	struct stat sb;

	ctx = malloc(sizeof (struct io_ctx));
	if (ctx == NULL)
		return ((struct io_ctx *)PyErr_NoMemory());

	ctx->io_fp = fopen(fn, "r");
	if (ctx->io_fp == NULL) {
		free(ctx);
		return ((struct io_ctx *)pybsm_error(errno));
	}
	ctx->io_fd = fileno(ctx->io_fp);

	if (fstat(ctx->io_fd, &sb) != 0) {
		free(ctx);
		return ((struct io_ctx *)pybsm_error(errno));
	}
	ctx->io_flags = 0;
	if (S_ISREG(sb.st_mode)) 
		ctx->io_flags |= IOFLAG_AUDITFILE;
	else
		ctx->io_flags |= IOFLAG_AUDITPIPE;

	return (ctx);	
}

static void
free_ctx(struct io_ctx *ctx)
{
	close(ctx->io_fd);
	free(ctx);
}

static PyObject *
io_init_py(__unused PyObject *unself, PyObject* args)
{
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	char *filename = NULL;
	struct io_ctx *ctx;
	
	if (!PyArg_ParseTuple(args, "O|s:__init__", &self, &filename))
		return (NULL);

	if (filename == NULL)
		filename = "/dev/auditpipe";

	ctx = init_ctx(filename);
	if (ctx == NULL)
		return (NULL);
	cobj = PyCObject_FromVoidPtr(ctx, (void (*)(void*))free_ctx); 
	PyObject_SetAttrString(self, "_ctx", cobj);
	Py_RETURN_NONE;
}

static PyObject *
io_destroy_py(__unused PyObject *unself, __unused PyObject* args)
{
	PyObject *self = NULL;
	PyObject *cobj = NULL;
	struct io_ctx *ctx;

	if (!PyArg_ParseTuple(args, "O:__del__", &self))
		Py_RETURN_NONE;
	cobj = PyObject_GetAttrString(self, "_ctx");
	if (cobj == NULL)
		Py_RETURN_NONE;
	ctx  = PyCObject_AsVoidPtr(cobj);
	free_ctx(ctx);
	Py_RETURN_NONE;
}

PyMODINIT_FUNC
initpybsm(void)
{
	PyMethodDef *def;
	PyObject *module, *moduleDict, *classDict, *className, *class;

	/*
	 * Init the pybsm module and its methods
	 */
	module = Py_InitModule("pybsm", pybsmMethods);
	moduleDict = PyModule_GetDict(module);
	
	/*
	 * Init the pybsm.io base class.
	 */
	classDict = PyDict_New();
	className = PyString_FromString("io");
	class = PyClass_New(NULL, classDict, className);

	/*
	 * Add io class to pybsm.
	 */
	PyDict_SetItemString(moduleDict, "io", class);

	Py_DECREF(classDict);
	Py_DECREF(className);
	Py_DECREF(class);

	/*
	 * Add all the pybsm.io methods.
	 */
	for (def = ioMethods; def->ml_name != NULL; def++)
	{
		PyObject *func, *method;

		func = PyCFunction_New(def, NULL);
		method = PyMethod_New(func, NULL, class);
		PyDict_SetItemString(classDict, def->ml_name, method);
		Py_DECREF(func);
		Py_DECREF(method);
	}
}
