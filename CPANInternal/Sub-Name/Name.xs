/* Copyright (C) 2004, 2008  Matthijs van Duin.  All rights reserved.
 * This program is free software; you can redistribute it and/or modify 
 * it under the same terms as Perl itself.
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static MGVTBL subname_vtbl;

#ifndef PERL_MAGIC_ext
# define PERL_MAGIC_ext '~'
#endif

#ifndef SvMAGIC_set
#define SvMAGIC_set(sv, val) (SvMAGIC(sv) = (val))
#endif


MODULE = Sub::Name  PACKAGE = Sub::Name

PROTOTYPES: DISABLE

void
subname(name, sub)
	char *name
	SV *sub
    PREINIT:
	CV *cv = NULL;
	GV *gv;
	HV *stash = CopSTASH(PL_curcop);
	char *s, *end = NULL, saved;
    PPCODE:
	if (!SvROK(sub) && SvGMAGICAL(sub))
		mg_get(sub);
	if (SvROK(sub))
		cv = (CV *) SvRV(sub);
	else if (SvTYPE(sub) == SVt_PVGV)
		cv = GvCVu(sub);
	else if (!SvOK(sub))
		croak(PL_no_usym, "a subroutine");
	else if (PL_op->op_private & HINT_STRICT_REFS)
		croak(PL_no_symref, SvPV_nolen(sub), "a subroutine");
	else if ((gv = gv_fetchpv(SvPV_nolen(sub), FALSE, SVt_PVCV)))
		cv = GvCVu(gv);
	if (!cv)
		croak("Undefined subroutine %s", SvPV_nolen(sub));
	if (SvTYPE(cv) != SVt_PVCV && SvTYPE(cv) != SVt_PVFM)
		croak("Not a subroutine reference");
	for (s = name; *s++; ) {
		if (*s == ':' && s[-1] == ':')
			end = ++s;
		else if (*s && s[-1] == '\'')
			end = s;
	}
	s--;
	if (end) {
		saved = *end;
		*end = 0;
		stash = GvHV(gv_fetchpv(name, TRUE, SVt_PVHV));
		*end = saved;
		name = end;
	}
	gv = (GV *) newSV(0);
	gv_init(gv, stash, name, s - name, TRUE);
#ifndef USE_5005THREADS
	if (CvPADLIST(cv)) {
		/* cheap way to refcount the gv */
		av_store((AV *) AvARRAY(CvPADLIST(cv))[0], 0, (SV *) gv);
	} else
#endif
	{
		/* expensive way to refcount the gv */
		MAGIC *mg = SvMAGIC(cv);
		while (mg && mg->mg_virtual != &subname_vtbl)
			mg = mg->mg_moremagic;
		if (!mg) {
			Newz(702, mg, 1, MAGIC);
			mg->mg_moremagic = SvMAGIC(cv);
			mg->mg_type = PERL_MAGIC_ext;
			mg->mg_virtual = &subname_vtbl;
			SvMAGIC_set(cv, mg);
		}
		if (mg->mg_flags & MGf_REFCOUNTED)
			SvREFCNT_dec(mg->mg_obj);
		mg->mg_flags |= MGf_REFCOUNTED;
		mg->mg_obj = (SV *) gv;
	}
	CvGV(cv) = gv;
	PUSHs(sub);
