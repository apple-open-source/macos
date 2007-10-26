#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>


#include "iconv_ces_if.h"

int iconv_cesmod_handler(module_t mod, int type, void *data);
int iconv_ces_open(const char *cesname, struct iconv_ces **cespp);
int iconv_ces_close(struct iconv_ces *ces);

/*
 * Character encoding scheme implementation
 */

static TAILQ_HEAD(, iconv_ces_class) iconv_ces_list;

static int
iconv_ces_lookup(const char *name, struct iconv_ces_class **cespp)
{
	struct iconv_ces_class *cesd;

	TAILQ_FOREACH(cesd, &iconv_ces_list, cd_link) {
		if (iconv_lookupcp(ICONV_CES_NAMES(cesd), name) == 0) {
			if (cespp)
				*cespp = cesd;
			return 0;
		}
	}
	return ENOENT;
}

static int
iconv_register_ces(struct iconv_ces_class *cesd)
{
	kobj_class_compile((struct kobj_class*)cesd);
	cesd->refs++;
	TAILQ_INSERT_TAIL(&iconv_ces_list, cesd, cd_link);
	return 0;
}

static int
iconv_unregister_ces(struct iconv_ces_class *cesd)
{
	if (cesd->refs > 1) {
		ICDEBUG("ces have %d referenses left\n", cesd->refs);
		return EBUSY;
	}
	TAILQ_REMOVE(&iconv_ces_list, cesd, cd_link);
	kobj_class_free((struct kobj_class*)cesd);
	return 0;
}

PRIVSYM int
iconv_ces_initstub(struct iconv_ces_class *cesd)
{
	#pragma unused(cesd)
	return 0;
}

PRIVSYM int
iconv_ces_donestub(struct iconv_ces_class *cesd)
{
	#pragma unused(cesd)
	return 0;
}

PRIVSYM int iconv_cesmod_handler(module_t mod, int type, void *data)
{
	#pragma unused(mod)
	struct iconv_ces_class *cesd = data;
	int error;

	switch (type) {
	    case MOD_LOAD:
		error = iconv_register_ces(cesd);
		if (error)
			break;
		error = ICONV_CES_INIT(cesd);
		if (error)
			iconv_unregister_ces(cesd);
		break;
	    case MOD_UNLOAD:
		ICONV_CES_DONE(cesd);
		error = iconv_unregister_ces(cesd);
		break;
	    default:
		error = EINVAL;
	}
	return error;
}

PRIVSYM int iconv_ces_open(const char *cesname, struct iconv_ces **cespp)
{
	struct iconv_ces_class *cesd;
	struct iconv_ces *ces;
	int error;

	error = iconv_ces_lookup(cesname, &cesd);
	if (error) {
		ICDEBUG("no ces %s found\n", cesname);
		error = iconv_ces_lookup("table", &cesd);
		if (error)
			return error;
		ICDEBUG("got table ces '%s'\n", cesname);
	}
	ces = (struct iconv_ces *)kobj_create((struct kobj_class*)cesd, M_ICONV);
	error = ICONV_CES_OPEN(ces, cesname);
	if (error) {
		kobj_delete((struct kobj*)ces, M_ICONV);
		return error;
	}
	*cespp = ces;
	return 0;
}

PRIVSYM int iconv_ces_close(struct iconv_ces *ces)
{
	if (ces == NULL)
		return EINVAL;
	ICONV_CES_CLOSE(ces);
	kobj_delete((struct kobj*)ces, M_ICONV);
	return 0;
}

#if 0
static void
iconv_ces_reset_func(struct iconv_ces *ces)
{
	ICONV_CES_CLOSE(ces);
	ICONV_CES_OPEN(ces, NULL);
}
#endif

/*ARGSUSED*/
PRIVSYM 
void iconv_ces_noreset(struct iconv_ces *ces)
{
	#pragma unused(ces)
}

#if 0
//PRIVSYM 
static int iconv_ces_nbits7(struct iconv_ces *ces)
{
	#pragma unused(ces)
	return 7;
}

//PRIVSYM
static int iconv_ces_nbits8(struct iconv_ces *ces)
{
	#pragma unused(ces)
	return 8;
}

//PRIVSYM int
static int iconv_ces_nbytes0(struct iconv_ces *ces)
{
	#pragma unused(ces)
	return 0;
}
#endif

static int
iconv_ces_handler(module_t mod, int type, void *data)
{
	#pragma unused(mod, data)
	int error = 0;

	switch (type) {
	    case MOD_LOAD:
		TAILQ_INIT(&iconv_ces_list);
		break;
	    case MOD_UNLOAD:
		break;
	    default:
		error = EINVAL;
	}
	return error;
}

static moduledata_t iconv_ces_mod = {
	"iconv_ces", iconv_ces_handler, NULL
};

DECLARE_MODULE(iconv_ces, iconv_ces_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
