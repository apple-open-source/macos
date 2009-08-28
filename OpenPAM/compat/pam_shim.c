#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <security/pam_appl.h>
#include <syslog.h>

const char libopenpam[] = "/usr/lib/libpam.2.dylib";


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_AUTH_##oldname = oldvalue,
enum {
#include "pam_shim_authenticate_flags.h"
};
#undef xlate

int
xlate_pam_shim_authenticate_flags(int flags)
{
	int newflags = 0;

#define xlate(oldname, oldvalue, newname, newvalue) \
    if (flags & COMPAT_AUTH_##oldname) newflags |= newname;
#include "pam_shim_authenticate_flags.h"
#undef xlate
	return newflags;
}


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_CHAUTH_##oldname = oldvalue,
enum {
#include "pam_shim_chauthtok_flags.h"
};
#undef xlate

int
xlate_pam_shim_chauthtok_flags(int flags)
{
	int newflags = 0;

#define xlate(oldname, oldvalue, newname, newvalue) \
    if (flags & COMPAT_CHAUTH_##oldname) newflags |= newname;
#include "pam_shim_chauthtok_flags.h"
#undef xlate
	return newflags;
}


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_SESSION_##oldname = oldvalue,
enum {
#include "pam_shim_session_flags.h"
};
#undef xlate

int
xlate_pam_shim_session_flags(int flags)
{
	int newflags = 0;

#define xlate(oldname, oldvalue, newname, newvalue) \
    if (flags & COMPAT_SESSION_##oldname) newflags |= newname;
#include "pam_shim_session_flags.h"
#undef xlate
	return newflags;
}


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_AUTH_##oldname = oldvalue,
enum {
#include "pam_shim_setcred_flags.h"
};
#undef xlate

int
xlate_pam_shim_setcred_flags(int flags)
{
	int newflags = 0;

#define xlate(oldname, oldvalue, newname, newvalue) \
    if (flags & COMPAT_AUTH_##oldname) newflags |= newname;
#include "pam_shim_setcred_flags.h"
#undef xlate
	return newflags;
}


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_##oldname = oldvalue,
enum {
#include "pam_shim_retval.h"
};
#undef xlate

int
xlate_pam_shim_retval(int retval)
{
#define xlate(oldname, oldvalue, newname, newvalue) \
	case COMPAT_##oldname: return newname;
	switch (retval) {
#include "pam_shim_retval.h"
	default: return retval;
	}
#undef xlate
}


#define xlate(oldname, oldvalue, newname, newvalue) \
    COMPAT_##oldname = oldvalue,
enum {
#include "pam_shim_item_type.h"
};
#undef xlate

int
xlate_pam_shim_item_type(int item_type)
{
#define xlate(oldname, oldvalue, newname, newvalue) \
	case COMPAT_##oldname: return newname;
	switch (item_type) {
#include "pam_shim_item_type.h"
	default: return item_type;
	}
#undef xlate
}


void dlfailure (const char *caller)
{
	syslog(LOG_ERR, "libpam.1.dylib failed calling %s: %s", caller, dlerror());
	abort();
}


typedef int (*pam_start_func)(const char *, const char *, const struct pam_conv *, pam_handle_t **);

int
pam_start(const char *service_name, const char *user, const struct pam_conv *pam_conversation, pam_handle_t **pamh)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_start");
	pam_start_func func = dlsym(openpam, "pam_start");
	if (NULL == func) dlfailure("pam_start");
	return xlate_pam_shim_retval(func(service_name, user, pam_conversation, pamh));
}


typedef int (*pam_end_func)(pam_handle_t *, int);

int
pam_end(pam_handle_t *pamh, int pam_status)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_end");
	pam_end_func func = dlsym(openpam, "pam_end");
	if (NULL == func) dlfailure("pam_end");
	return xlate_pam_shim_retval(func(pamh, pam_status));
}


typedef int (*pam_authenticate_func)(pam_handle_t *, int);

int
pam_authenticate(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_authenticate_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_authenticate");
	pam_authenticate_func func = dlsym(openpam, "pam_authenticate");
	if (NULL == func) dlfailure("pam_authenticate");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_setcred_func)(pam_handle_t *, int);

int
pam_setcred(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_authenticate_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_setcred");
	pam_setcred_func func = dlsym(openpam, "pam_setcred");
	if (NULL == func) dlfailure("pam_setcred");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_acct_mgmt_func)(pam_handle_t *, int);

int
pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_authenticate_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_acct_mgmt");
	pam_acct_mgmt_func func = dlsym(openpam, "pam_acct_mgmt");
	if (NULL == func) dlfailure("pam_acct_mgmt");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_chauthtok_func)(pam_handle_t *, int);

int
pam_chauthtok(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_chauthtok_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_chauthtok");
	pam_chauthtok_func func = dlsym(openpam, "pam_chauthtok");
	if (NULL == func) dlfailure("pam_chauthtok");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_open_session_func)(pam_handle_t *, int);

int
pam_open_session(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_session_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_open_session");
	pam_open_session_func func = dlsym(openpam, "pam_open_session");
	if (NULL == func) dlfailure("pam_open_session");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_close_session_func)(pam_handle_t *, int);

int
pam_close_session(pam_handle_t *pamh, int flags)
{
	int newflags = xlate_pam_shim_session_flags(flags);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_close_session");
	pam_close_session_func func = dlsym(openpam, "pam_close_session");
	if (NULL == func) dlfailure("pam_close_session");
	return xlate_pam_shim_retval(func(pamh, newflags));
}


typedef int (*pam_set_item_func)(pam_handle_t *, int, const void *);

int
pam_set_item(pam_handle_t *pamh, int item_type, const void *item)
{
	int newitemtype = xlate_pam_shim_item_type(item_type);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_set_item");
	pam_set_item_func func = dlsym(openpam, "pam_set_item");
	if (NULL == func) dlfailure("pam_set_item");
	return xlate_pam_shim_retval(func(pamh, newitemtype, item));
}


typedef int (*pam_get_item_func)(const pam_handle_t *, int, const void **);

int
pam_get_item(const pam_handle_t *pamh, int item_type, const void **item)
{
	int newitemtype = xlate_pam_shim_item_type(item_type);
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_get_item");
	pam_get_item_func func = dlsym(openpam, "pam_get_item");
	if (NULL == func) dlfailure("pam_get_item");
	return xlate_pam_shim_retval(func(pamh, newitemtype, item));
}


typedef int (*misc_conv_func)(int, const struct pam_message **, struct pam_response **, void *);

int
misc_conv(int num_msg, const struct pam_message **msgm, struct pam_response **response, void *appdata_ptr)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("misc_conv");
	misc_conv_func func = dlsym(openpam, "openpam_ttyconv");
	if (NULL == func) dlfailure("misc_conv");
	return xlate_pam_shim_retval(func(num_msg, msgm, response, appdata_ptr));
}


typedef const char * (*pam_strerr_func)(const pam_handle_t *, int);

const char *
pam_strerror(const pam_handle_t *pamh, int pam_error)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_strerror");
	pam_strerr_func func = dlsym(openpam, "pam_strerror");
	if (NULL == func) dlfailure("pam_strerror");
	return func(pamh, pam_error);
}


typedef int (*pam_putenv_func)(pam_handle_t *, const char *);

int
pam_putenv(pam_handle_t *pamh, const char *name_value)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_putenv");
	pam_putenv_func func = dlsym(openpam, "pam_putenv");
	if (NULL == func) dlfailure("pam_putenv");
	return xlate_pam_shim_retval(func(pamh, name_value));
}


typedef const char * (*pam_getenv_func)(pam_handle_t *, const char *);

const char *
pam_getenv(pam_handle_t *pamh, const char *name)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_getenv");
	pam_getenv_func func = dlsym(openpam, "pam_getenv");
	if (NULL == func) dlfailure("pam_getenv");
	return func(pamh, name);
}


typedef char ** (*pam_getenvlist_func)(pam_handle_t *);

char **
pam_getenvlist(pam_handle_t *pamh)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_getenvlist");
	pam_getenvlist_func func = dlsym(openpam, "pam_getenvlist");
	if (NULL == func) dlfailure("pam_getenvlist");
	return func(pamh);
}


typedef int (*pam_get_data_func)(pam_handle_t *, const char *, const void **);
int aliased_pam_get_data(pam_handle_t *pamh, const char *module_data_name, const void **data) __asm("_pam_get_data");

int
aliased_pam_get_data(pam_handle_t *pamh, const char *module_data_name, const void **data)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_get_data");
	pam_get_data_func func = dlsym(openpam, "pam_get_data");
	if (NULL == func) dlfailure("pam_get_data");
	return xlate_pam_shim_retval(func(pamh, module_data_name, data));
}


typedef void (*pam_set_data_cleanup_func)(pam_handle_t, void *, int);
typedef int (*pam_set_data_func)(pam_handle_t *, const char *, void *, pam_set_data_cleanup_func);
int aliased_pam_set_data(pam_handle_t *pamh, const char *modue_data_name, void *data, pam_set_data_cleanup_func cleanup) __asm("_pam_set_data");

int
aliased_pam_set_data(pam_handle_t *pamh, const char *modue_data_name, void *data, pam_set_data_cleanup_func cleanup)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_set_data");
	pam_set_data_func func = dlsym(openpam, "pam_set_data");
	if (NULL == func) dlfailure("pam_set_data");
	return xlate_pam_shim_retval(func(pamh, modue_data_name, data, cleanup));
}


typedef int (*pam_get_user_func)(pam_handle_t *, char **, const char *);
int aliased_pam_get_user(pam_handle_t *pamh, char **user, const char *prompt) __asm("_pam_get_user");

int
aliased_pam_get_user(pam_handle_t *pamh, char **user, const char *prompt)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_get_user");
	pam_get_user_func func = dlsym(openpam, "pam_get_user");
	if (NULL == func) dlfailure("pam_get_user");
	return xlate_pam_shim_retval(func(pamh, user, prompt));
}


typedef int (*pam_prompt_func)(const pam_handle_t *, int, char **, const char *, ...);

int
pam_prompt(pam_handle_t *pamh, int style, const char *prompt, char **user_msg)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_prompt");
	pam_prompt_func func = dlsym(openpam, "pam_prompt");
	if (NULL == func) dlfailure("pam_prompt");
	return xlate_pam_shim_retval(func(pamh, style, user_msg, "%s", prompt));
}


typedef int (*pam_get_pass_func)(pam_handle_t *, int, const char **, const char *);

int
pam_get_pass(pam_handle_t *pamh, const char **passp, const char *prompt, __unused int options)
{
	void *openpam = dlopen(libopenpam, RTLD_LOCAL | RTLD_LAZY | RTLD_FIRST);
	if (NULL == openpam) dlfailure("pam_get_pass");
	pam_get_pass_func func = dlsym(openpam, "pam_get_authtok");
	if (NULL == func) dlfailure("pam_get_authtok");
	return xlate_pam_shim_retval(func(pamh, PAM_AUTHTOK, passp, prompt));
}
