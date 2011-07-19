/* This file is part of the Variable::Magic Perl module.
 * See http://search.cpan.org/dist/Variable-Magic/ */

#include <stdarg.h> /* <va_list>, va_{start,arg,end}, ... */

#include <stdio.h>  /* sprintf() */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define __PACKAGE__ "Variable::Magic"

#ifndef VMG_PERL_PATCHLEVEL
# ifdef PERL_PATCHNUM
#  define VMG_PERL_PATCHLEVEL PERL_PATCHNUM
# else
#  define VMG_PERL_PATCHLEVEL 0
# endif
#endif

#define VMG_HAS_PERL(R, V, S) (PERL_REVISION > (R) || (PERL_REVISION == (R) && (PERL_VERSION > (V) || (PERL_VERSION == (V) && (PERL_SUBVERSION >= (S))))))

#define VMG_HAS_PERL_BRANCH(R, V, S) (PERL_REVISION == (R) && PERL_VERSION == (V) && PERL_SUBVERSION >= (S))

#define VMG_HAS_PERL_MAINT(R, V, S, P) (PERL_REVISION == (R) && PERL_VERSION == (V) && (VMG_PERL_PATCHLEVEL >= (P) || (!VMG_PERL_PATCHLEVEL && PERL_SUBVERSION >= (S))))

/* --- Threads and multiplicity -------------------------------------------- */

#ifndef NOOP
# define NOOP
#endif

#ifndef dNOOP
# define dNOOP
#endif

/* Safe unless stated otherwise in Makefile.PL */
#ifndef VMG_FORKSAFE
# define VMG_FORKSAFE 1
#endif

#ifndef VMG_MULTIPLICITY
# if defined(MULTIPLICITY) || defined(PERL_IMPLICIT_CONTEXT)
#  define VMG_MULTIPLICITY 1
# else
#  define VMG_MULTIPLICITY 0
# endif
#endif
#if VMG_MULTIPLICITY && !defined(tTHX)
# define tTHX PerlInterpreter*
#endif

#if VMG_MULTIPLICITY && defined(USE_ITHREADS) && defined(dMY_CXT) && defined(MY_CXT) && defined(START_MY_CXT) && defined(MY_CXT_INIT) && (defined(MY_CXT_CLONE) || defined(dMY_CXT_SV))
# define VMG_THREADSAFE 1
# ifndef MY_CXT_CLONE
#  define MY_CXT_CLONE \
    dMY_CXT_SV;                                                      \
    my_cxt_t *my_cxtp = (my_cxt_t*)SvPVX(newSV(sizeof(my_cxt_t)-1)); \
    Copy(INT2PTR(my_cxt_t*, SvUV(my_cxt_sv)), my_cxtp, 1, my_cxt_t); \
    sv_setuv(my_cxt_sv, PTR2UV(my_cxtp))
# endif
#else
# define VMG_THREADSAFE 0
# undef  dMY_CXT
# define dMY_CXT      dNOOP
# undef  MY_CXT
# define MY_CXT       vmg_globaldata
# undef  START_MY_CXT
# define START_MY_CXT STATIC my_cxt_t MY_CXT;
# undef  MY_CXT_INIT
# define MY_CXT_INIT  NOOP
# undef  MY_CXT_CLONE
# define MY_CXT_CLONE NOOP
#endif

#if VMG_THREADSAFE

STATIC SV *vmg_clone(pTHX_ SV *sv, tTHX owner) {
#define vmg_clone(P, O) vmg_clone(aTHX_ (P), (O))
 SV *dupsv;

#if VMG_HAS_PERL(5, 13, 2)
 CLONE_PARAMS *param = Perl_clone_params_new(owner, aTHX);

 dupsv = sv_dup(sv, param);

 Perl_clone_params_del(param);
#else
 CLONE_PARAMS param;

 param.stashes    = NULL; /* don't need it unless sv is a PVHV */
 param.flags      = 0;
 param.proto_perl = owner;

 dupsv = sv_dup(sv, &param);
#endif

 return SvREFCNT_inc(dupsv);
}

#endif /* VMG_THREADSAFE */

/* --- Compatibility ------------------------------------------------------- */

#ifndef Newx
# define Newx(v, n, c) New(0, v, n, c)
#endif

#ifndef SvMAGIC_set
# define SvMAGIC_set(sv, val) (SvMAGIC(sv) = (val))
#endif

#ifndef SvRV_const
# define SvRV_const(sv) SvRV((SV *) sv)
#endif

#ifndef SvREFCNT_inc_simple_void
# define SvREFCNT_inc_simple_void(sv) SvREFCNT_inc(sv)
#endif

#ifndef mPUSHu
# define mPUSHu(U) PUSHs(sv_2mortal(newSVuv(U)))
#endif

#ifndef PERL_MAGIC_ext
# define PERL_MAGIC_ext '~'
#endif

#ifndef PERL_MAGIC_tied
# define PERL_MAGIC_tied 'P'
#endif

#ifndef MGf_COPY
# define MGf_COPY 0
#endif

#ifndef MGf_DUP
# define MGf_DUP 0
#endif

#ifndef MGf_LOCAL
# define MGf_LOCAL 0
#endif

#ifndef IN_PERL_COMPILETIME
# define IN_PERL_COMPILETIME (PL_curcop == &PL_compiling)
#endif

/* uvar magic and Hash::Util::FieldHash were commited with 28419, but we only
 * enable them on 5.10 */
#if VMG_HAS_PERL(5, 10, 0)
# define VMG_UVAR 1
#else
# define VMG_UVAR 0
#endif

/* Applied to dev-5.9 as 25854, integrated to maint-5.8 as 28160, partially
 * reverted to dev-5.11 as 9cdcb38b */
#if VMG_HAS_PERL_MAINT(5, 8, 9, 28160) || VMG_HAS_PERL_MAINT(5, 9, 3, 25854) || VMG_HAS_PERL(5, 10, 0)
# ifndef VMG_COMPAT_ARRAY_PUSH_NOLEN
/* This branch should only apply for perls before the official 5.11.0 release.
 * Makefile.PL takes care of the higher ones. */
#  define VMG_COMPAT_ARRAY_PUSH_NOLEN 1
# endif
# ifndef VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID
#  define VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID 1
# endif
#else
# ifndef VMG_COMPAT_ARRAY_PUSH_NOLEN
#  define VMG_COMPAT_ARRAY_PUSH_NOLEN 0
# endif
# ifndef VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID
#  define VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID 0
# endif
#endif

/* Applied to dev-5.11 as 34908 */
#if VMG_HAS_PERL_MAINT(5, 11, 0, 34908) || VMG_HAS_PERL(5, 12, 0)
# define VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID 1
#else
# define VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID 0
#endif

/* Applied to dev-5.9 as 31473 (see #43357), integrated to maint-5.8 as 32542 */
#if VMG_HAS_PERL_MAINT(5, 8, 9, 32542) || VMG_HAS_PERL_MAINT(5, 9, 5, 31473) || VMG_HAS_PERL(5, 10, 0)
# define VMG_COMPAT_ARRAY_UNDEF_CLEAR 1
#else
# define VMG_COMPAT_ARRAY_UNDEF_CLEAR 0
#endif

#if VMG_HAS_PERL_MAINT(5, 11, 0, 32969) || VMG_HAS_PERL(5, 12, 0)
# define VMG_COMPAT_SCALAR_LENGTH_NOLEN 1
#else
# define VMG_COMPAT_SCALAR_LENGTH_NOLEN 0
#endif

#if VMG_HAS_PERL(5, 13, 2)
# define VMG_COMPAT_GLOB_GET 1
#else
# define VMG_COMPAT_GLOB_GET 0
#endif

/* ... Bug-free mg_magical ................................................. */

/* See the discussion at http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2008-01/msg00036.html. This version is specialized to our needs. */

#if VMG_UVAR

STATIC void vmg_sv_magicuvar(pTHX_ SV *sv, const char *uf, I32 len) {
#define vmg_sv_magicuvar(S, U, L) vmg_sv_magicuvar(aTHX_ (S), (U), (L))
 const MAGIC* mg;
 sv_magic(sv, NULL, PERL_MAGIC_uvar, uf, len);
 /* uvar magic has set and get magic, hence this has set SVs_GMG and SVs_SMG. */
 if ((mg = SvMAGIC(sv))) {
  SvRMAGICAL_off(sv);
  do {
   const MGVTBL* const vtbl = mg->mg_virtual;
   if (vtbl) {
    if (vtbl->svt_clear) {
     SvRMAGICAL_on(sv);
     break;
    }
   }
  } while ((mg = mg->mg_moremagic));
 }
}

#endif /* VMG_UVAR */

/* ... Safe version of call_sv() ........................................... */

#define VMG_SAVE_LAST_CX (!VMG_HAS_PERL(5, 8, 4) || VMG_HAS_PERL(5, 9, 5))

STATIC I32 vmg_call_sv(pTHX_ SV *sv, I32 flags, I32 destructor) {
#define vmg_call_sv(S, F, D) vmg_call_sv(aTHX_ (S), (F), (D))
 I32 ret, cxix = 0, in_eval = 0;
#if VMG_SAVE_LAST_CX
 PERL_CONTEXT saved_cx;
#endif
 SV *old_err = NULL;

 if (SvTRUE(ERRSV)) {
  old_err = ERRSV;
  ERRSV   = newSV(0);
 }

 if (cxstack_ix < cxstack_max) {
  cxix = cxstack_ix + 1;
  if (destructor && CxTYPE(cxstack + cxix) == CXt_EVAL)
   in_eval = 1;
 }

#if VMG_SAVE_LAST_CX
 /* The last popped context will be reused by call_sv(), but our callers may
  * still need its previous value. Back it up so that it isn't clobbered. */
 saved_cx = cxstack[cxix];
#endif

 ret = call_sv(sv, flags | G_EVAL);

#if VMG_SAVE_LAST_CX
 cxstack[cxix] = saved_cx;
#endif

 if (SvTRUE(ERRSV)) {
  if (old_err) {
   sv_setsv(old_err, ERRSV);
   SvREFCNT_dec(ERRSV);
   ERRSV = old_err;
  }
  if (IN_PERL_COMPILETIME) {
   if (!PL_in_eval) {
    if (PL_errors)
     sv_catsv(PL_errors, ERRSV);
    else
     Perl_warn(aTHX_ "%s", SvPV_nolen(ERRSV));
    SvCUR_set(ERRSV, 0);
   }
#if VMG_HAS_PERL(5, 10, 0) || defined(PL_parser)
   if (PL_parser)
    ++PL_parser->error_count;
#elif defined(PL_error_count)
   ++PL_error_count;
#else
   ++PL_Ierror_count;
#endif
   } else if (!in_eval)
    croak(NULL);
 } else {
  if (old_err) {
   SvREFCNT_dec(ERRSV);
   ERRSV = old_err;
  }
 }

 return ret;
}

/* --- Stolen chunk of B --------------------------------------------------- */

typedef enum {
 OPc_NULL   = 0,
 OPc_BASEOP = 1,
 OPc_UNOP   = 2,
 OPc_BINOP  = 3,
 OPc_LOGOP  = 4,
 OPc_LISTOP = 5,
 OPc_PMOP   = 6,
 OPc_SVOP   = 7,
 OPc_PADOP  = 8,
 OPc_PVOP   = 9,
 OPc_LOOP   = 10,
 OPc_COP    = 11,
 OPc_MAX    = 12
} opclass;

STATIC const char *const vmg_opclassnames[] = {
 "B::NULL",
 "B::OP",
 "B::UNOP",
 "B::BINOP",
 "B::LOGOP",
 "B::LISTOP",
 "B::PMOP",
 "B::SVOP",
 "B::PADOP",
 "B::PVOP",
 "B::LOOP",
 "B::COP"
};

STATIC opclass vmg_opclass(const OP *o) {
#if 0
 if (!o)
  return OPc_NULL;
#endif

 if (o->op_type == 0)
  return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

 if (o->op_type == OP_SASSIGN)
  return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

 if (o->op_type == OP_AELEMFAST) {
  if (o->op_flags & OPf_SPECIAL)
   return OPc_BASEOP;
  else
#ifdef USE_ITHREADS
   return OPc_PADOP;
#else
   return OPc_SVOP;
#endif
 }

#ifdef USE_ITHREADS
 if (o->op_type == OP_GV || o->op_type == OP_GVSV || o->op_type == OP_RCATLINE)
  return OPc_PADOP;
#endif

 switch (PL_opargs[o->op_type] & OA_CLASS_MASK) {
  case OA_BASEOP:
   return OPc_BASEOP;
  case OA_UNOP:
   return OPc_UNOP;
  case OA_BINOP:
   return OPc_BINOP;
  case OA_LOGOP:
   return OPc_LOGOP;
  case OA_LISTOP:
   return OPc_LISTOP;
  case OA_PMOP:
   return OPc_PMOP;
  case OA_SVOP:
   return OPc_SVOP;
  case OA_PADOP:
   return OPc_PADOP;
  case OA_PVOP_OR_SVOP:
   return (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF)) ? OPc_SVOP : OPc_PVOP;
  case OA_LOOP:
   return OPc_LOOP;
  case OA_COP:
   return OPc_COP;
  case OA_BASEOP_OR_UNOP:
   return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;
  case OA_FILESTATOP:
   return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
#ifdef USE_ITHREADS
           (o->op_flags & OPf_REF) ? OPc_PADOP : OPc_BASEOP);
#else
           (o->op_flags & OPf_REF) ? OPc_SVOP : OPc_BASEOP);
#endif
  case OA_LOOPEXOP:
   if (o->op_flags & OPf_STACKED)
    return OPc_UNOP;
   else if (o->op_flags & OPf_SPECIAL)
    return OPc_BASEOP;
   else
    return OPc_PVOP;
 }

 return OPc_BASEOP;
}

/* --- Error messages ------------------------------------------------------ */

STATIC const char vmg_invalid_wiz[]    = "Invalid wizard object";
STATIC const char vmg_wrongargnum[]    = "Wrong number of arguments";
STATIC const char vmg_argstorefailed[] = "Error while storing arguments";

/* --- Signatures ---------------------------------------------------------- */

#define SIG_WZO ((U16) (0x3891))
#define SIG_WIZ ((U16) (0x3892))

/* --- MGWIZ structure ----------------------------------------------------- */

typedef struct {
 MGVTBL *vtbl;

 U8 opinfo;
 U8 uvar;

 SV *cb_data;
 SV *cb_get, *cb_set, *cb_len, *cb_clear, *cb_free;
#if MGf_COPY
 SV *cb_copy;
#endif /* MGf_COPY */
#if MGf_DUP
 SV *cb_dup;
#endif /* MGf_DUP */
#if MGf_LOCAL
 SV *cb_local;
#endif /* MGf_LOCAL */
#if VMG_UVAR
 SV *cb_fetch, *cb_store, *cb_exists, *cb_delete;
#endif /* VMG_UVAR */

#if VMG_MULTIPLICITY
 tTHX owner;
#endif /* VMG_MULTIPLICITY */
} MGWIZ;

STATIC void vmg_mgwiz_free(pTHX_ MGWIZ *w) {
#define vmg_mgwiz_free(W) vmg_mgwiz_free(aTHX_ (W))
 if (!w)
  return;

 if (w->cb_data)   SvREFCNT_dec(w->cb_data);
 if (w->cb_get)    SvREFCNT_dec(w->cb_get);
 if (w->cb_set)    SvREFCNT_dec(w->cb_set);
 if (w->cb_len)    SvREFCNT_dec(w->cb_len);
 if (w->cb_clear)  SvREFCNT_dec(w->cb_clear);
 if (w->cb_free)   SvREFCNT_dec(w->cb_free);
#if MGf_COPY
 if (w->cb_copy)   SvREFCNT_dec(w->cb_copy);
#endif /* MGf_COPY */
#if 0 /* MGf_DUP */
 if (w->cb_dup)    SvREFCNT_dec(w->cb_dup);
#endif /* MGf_DUP */
#if MGf_LOCAL
 if (w->cb_local)  SvREFCNT_dec(w->cb_local);
#endif /* MGf_LOCAL */
#if VMG_UVAR
 if (w->cb_fetch)  SvREFCNT_dec(w->cb_fetch);
 if (w->cb_store)  SvREFCNT_dec(w->cb_store);
 if (w->cb_exists) SvREFCNT_dec(w->cb_exists);
 if (w->cb_delete) SvREFCNT_dec(w->cb_delete);
#endif /* VMG_UVAR */

 Safefree(w->vtbl);
 Safefree(w);

 return;
}

#if VMG_THREADSAFE

#define VMG_CLONE_CB(N) \
 z->cb_ ## N = (w->cb_ ## N) ? vmg_clone(w->cb_ ## N, w->owner) \
                             : NULL;

STATIC MGWIZ *vmg_mgwiz_clone(pTHX_ const MGWIZ *w) {
#define vmg_mgwiz_clone(W) vmg_mgwiz_clone(aTHX_ (W))
 MGVTBL *t;
 MGWIZ *z;

 if (!w)
  return NULL;

 Newx(t, 1, MGVTBL);
 Copy(w->vtbl, t, 1, MGVTBL);

 Newx(z, 1, MGWIZ);

 z->vtbl   = t;
 z->uvar   = w->uvar;
 z->opinfo = w->opinfo;

 VMG_CLONE_CB(data);
 VMG_CLONE_CB(get);
 VMG_CLONE_CB(set);
 VMG_CLONE_CB(len);
 VMG_CLONE_CB(clear);
 VMG_CLONE_CB(free);
#if MGf_COPY
 VMG_CLONE_CB(copy);
#endif /* MGf_COPY */
#if MGf_DUP
 VMG_CLONE_CB(dup);
#endif /* MGf_DUP */
#if MGf_LOCAL
 VMG_CLONE_CB(local);
#endif /* MGf_LOCAL */
#if VMG_UVAR
 VMG_CLONE_CB(fetch);
 VMG_CLONE_CB(store);
 VMG_CLONE_CB(exists);
 VMG_CLONE_CB(delete);
#endif /* VMG_UVAR */

 z->owner = aTHX;

 return z;
}

#endif /* VMG_THREADSAFE */

/* --- Context-safe global data -------------------------------------------- */

#if VMG_THREADSAFE

#define PTABLE_NAME        ptable
#define PTABLE_VAL_FREE(V) vmg_mgwiz_free(V)

#define pPTBL  pTHX
#define pPTBL_ pTHX_
#define aPTBL  aTHX
#define aPTBL_ aTHX_

#include "ptable.h"

#define ptable_store(T, K, V) ptable_store(aTHX_ (T), (K), (V))
#define ptable_clear(T)       ptable_clear(aTHX_ (T))
#define ptable_free(T)        ptable_free(aTHX_ (T))

#endif /* VMG_THREADSAFE */

#define MY_CXT_KEY __PACKAGE__ "::_guts" XS_VERSION

typedef struct {
#if VMG_THREADSAFE
 ptable *wizards;
 tTHX    owner;
#endif
 HV     *b__op_stashes[OPc_MAX];
} my_cxt_t;

START_MY_CXT

#if VMG_THREADSAFE

STATIC void vmg_ptable_clone(pTHX_ ptable_ent *ent, void *ud_) {
 my_cxt_t *ud = ud_;
 MGWIZ *w;

 if (ud->owner == aTHX)
  return;

 w = vmg_mgwiz_clone(ent->val);
 if (w)
  ptable_store(ud->wizards, ent->key, w);
}

#endif /* VMG_THREADSAFE */

/* --- Wizard objects ------------------------------------------------------ */

STATIC int vmg_wizard_free(pTHX_ SV *sv, MAGIC *mg);

STATIC MGVTBL vmg_wizard_vtbl = {
 NULL,            /* get */
 NULL,            /* set */
 NULL,            /* len */
 NULL,            /* clear */
 vmg_wizard_free, /* free */
#if MGf_COPY
 NULL,            /* copy */
#endif /* MGf_COPY */
#if MGf_DUP
 NULL,            /* dup */
#endif /* MGf_DUP */
#if MGf_LOCAL
 NULL,            /* local */
#endif /* MGf_LOCAL */
};

/* ... Wizard constructor .................................................. */

STATIC SV *vmg_wizard_new(pTHX_ const MGWIZ *w) {
#define vmg_wizard_new(W) vmg_wizard_new(aTHX_ (W))
 SV *wiz = newSVuv(PTR2IV(w));

 if (w) {
  MAGIC *mg = sv_magicext(wiz, NULL, PERL_MAGIC_ext, &vmg_wizard_vtbl, NULL, 0);
  mg->mg_private = SIG_WZO;
 }
 SvREADONLY_on(wiz);

 return wiz;
}

STATIC const SV *vmg_wizard_validate(pTHX_ const SV *wiz) {
#define vmg_wizard_validate(W) vmg_wizard_validate(aTHX_ (W))
 if (SvROK(wiz)) {
  wiz = SvRV_const(wiz);
  if (SvIOK(wiz))
   return wiz;
 }

 croak(vmg_invalid_wiz);
 /* Not reached */
 return NULL;
}

#define vmg_wizard_id(W)         SvIVX((const SV *) (W))
#define vmg_wizard_main_mgwiz(W) INT2PTR(const MGWIZ *, vmg_wizard_id(W))

/* ... Wizard destructor ................................................... */

STATIC int vmg_wizard_free(pTHX_ SV *sv, MAGIC *mg) {
 MGWIZ *w;

 if (PL_dirty) /* During global destruction, the context is already freed */
  return 0;

 w = (MGWIZ *) vmg_wizard_main_mgwiz(sv);

#if VMG_THREADSAFE
 {
  dMY_CXT;
  ptable_store(MY_CXT.wizards, w, NULL);
 }
#else /* VMG_THREADSAFE */
 vmg_mgwiz_free(w);
#endif /* !VMG_THREADSAFE */

 return 0;
}

#if VMG_THREADSAFE

STATIC const MGWIZ *vmg_wizard_mgwiz(pTHX_ const SV *wiz) {
#define vmg_wizard_mgwiz(W) vmg_wizard_mgwiz(aTHX_ ((const SV *) (W)))
 const MGWIZ *w;

 w = vmg_wizard_main_mgwiz(wiz);
 if (w->owner == aTHX)
  return w;

 {
  dMY_CXT;
  return ptable_fetch(MY_CXT.wizards, w);
 }
}

#else /* VMG_THREADSAFE */

#define vmg_wizard_mgwiz(W) vmg_wizard_main_mgwiz(W)

#endif /* !VMG_THREADSAFE */

/* --- User-level functions implementation --------------------------------- */

STATIC const MAGIC *vmg_find(const SV *sv, const SV *wiz) {
 const MAGIC *mg, *moremagic;
 IV wid;

 if (SvTYPE(sv) < SVt_PVMG)
  return NULL;

 wid = vmg_wizard_id(wiz);
 for (mg = SvMAGIC(sv); mg; mg = moremagic) {
  moremagic = mg->mg_moremagic;
  if (mg->mg_type == PERL_MAGIC_ext && mg->mg_private == SIG_WIZ) {
   IV zid = vmg_wizard_id(mg->mg_ptr);
   if (zid == wid)
    return mg;
  }
 }

 return NULL;
}

/* ... Construct private data .............................................. */

STATIC SV *vmg_data_new(pTHX_ SV *ctor, SV *sv, SV **args, I32 items) {
#define vmg_data_new(C, S, A, I) vmg_data_new(aTHX_ (C), (S), (A), (I))
 I32 i;
 SV *nsv;

 dSP;

 ENTER;
 SAVETMPS;

 PUSHMARK(SP);
 EXTEND(SP, items + 1);
 PUSHs(sv_2mortal(newRV_inc(sv)));
 for (i = 0; i < items; ++i)
  PUSHs(args[i]);
 PUTBACK;

 vmg_call_sv(ctor, G_SCALAR, 0);

 SPAGAIN;
 nsv = POPs;
#if VMG_HAS_PERL(5, 8, 3)
 SvREFCNT_inc_simple_void(nsv); /* Or it will be destroyed in FREETMPS */
#else
 nsv = sv_newref(nsv);          /* Workaround some bug in SvREFCNT_inc() */
#endif
 PUTBACK;

 FREETMPS;
 LEAVE;

 return nsv;
}

STATIC SV *vmg_data_get(pTHX_ SV *sv, const SV *wiz) {
#define vmg_data_get(S, W) vmg_data_get(aTHX_ (S), (W))
 const MAGIC *mg = vmg_find(sv, wiz);
 return mg ? mg->mg_obj : NULL;
} 

/* ... Magic cast/dispell .................................................. */

#if VMG_UVAR
STATIC I32 vmg_svt_val(pTHX_ IV, SV *);

STATIC void vmg_uvar_del(SV *sv, MAGIC *prevmagic, MAGIC *mg, MAGIC *moremagic) {
 if (prevmagic) {
  prevmagic->mg_moremagic = moremagic;
 } else {
  SvMAGIC_set(sv, moremagic);
 }
 mg->mg_moremagic = NULL;
 Safefree(mg->mg_ptr);
 Safefree(mg);
}
#endif /* VMG_UVAR */

STATIC UV vmg_cast(pTHX_ SV *sv, const SV *wiz, SV **args, I32 items) {
#define vmg_cast(S, W, A, I) vmg_cast(aTHX_ (S), (W), (A), (I))
 MAGIC       *mg, *moremagic = NULL;
 SV          *data;
 const MGWIZ *w;
 U32          oldgmg;

 if (vmg_find(sv, wiz))
  return 1;

 w = vmg_wizard_mgwiz(wiz);
 oldgmg = SvGMAGICAL(sv);

 data = (w->cb_data) ? vmg_data_new(w->cb_data, sv, args, items) : NULL;
 mg = sv_magicext(sv, data, PERL_MAGIC_ext, w->vtbl, (const char *) wiz, HEf_SVKEY);
 SvREFCNT_dec(data);
 mg->mg_private = SIG_WIZ;
#if MGf_COPY
 if (w->cb_copy)
  mg->mg_flags |= MGf_COPY;
#endif /* MGf_COPY */
#if 0 /* MGf_DUP */
 if (w->cb_dup)
  mg->mg_flags |= MGf_DUP;
#endif /* MGf_DUP */
#if MGf_LOCAL
 if (w->cb_local)
  mg->mg_flags |= MGf_LOCAL;
#endif /* MGf_LOCAL */

 if (SvTYPE(sv) < SVt_PVHV)
  goto done;

 /* The GMAGICAL flag only says that a hash is tied or has uvar magic - get
  * magic is actually never called for them. If the GMAGICAL flag was off before
  * calling sv_magicext(), the hash isn't tied and has no uvar magic. If it's
  * now on, then this wizard has get magic. Hence we can work around the
  * get/clear shortcoming by turning the GMAGICAL flag off. If the current magic
  * has uvar callbacks, it will be turned back on later. */
 if (!oldgmg && SvGMAGICAL(sv))
  SvGMAGICAL_off(sv);

#if VMG_UVAR
 if (w->uvar) {
  MAGIC *prevmagic;
  struct ufuncs uf[2];

  uf[0].uf_val   = vmg_svt_val;
  uf[0].uf_set   = NULL;
  uf[0].uf_index = 0;
  uf[1].uf_val   = NULL;
  uf[1].uf_set   = NULL;
  uf[1].uf_index = 0;

  /* One uvar magic in the chain is enough. */
  for (prevmagic = NULL, mg = SvMAGIC(sv); mg; prevmagic = mg, mg = moremagic) {
   moremagic = mg->mg_moremagic;
   if (mg->mg_type == PERL_MAGIC_uvar)
    break;
  }

  if (mg) { /* Found another uvar magic. */
   struct ufuncs *olduf = (struct ufuncs *) mg->mg_ptr;
   if (olduf->uf_val == vmg_svt_val) {
    /* It's our uvar magic, nothing to do. oldgmg was true. */
    goto done;
   } else {
    /* It's another uvar magic, backup it and replace it by ours. */
    uf[1] = *olduf;
    vmg_uvar_del(sv, prevmagic, mg, moremagic);
   }
  }

  vmg_sv_magicuvar(sv, (const char *) &uf, sizeof(uf));
  /* Our hash now carries uvar magic. The uvar/clear shortcoming has to be
   * handled by our uvar callback. */
 }
#endif /* VMG_UVAR */

done:
 return 1;
}

STATIC UV vmg_dispell(pTHX_ SV *sv, const SV *wiz) {
#define vmg_dispell(S, W) vmg_dispell(aTHX_ (S), (W))
#if VMG_UVAR
 U32 uvars = 0;
#endif /* VMG_UVAR */
 MAGIC *mg, *prevmagic, *moremagic = NULL;
 IV wid = vmg_wizard_id(wiz);

 if (SvTYPE(sv) < SVt_PVMG)
  return 0;

 for (prevmagic = NULL, mg = SvMAGIC(sv); mg; prevmagic = mg, mg = moremagic) {
  moremagic = mg->mg_moremagic;
  if (mg->mg_type == PERL_MAGIC_ext && mg->mg_private == SIG_WIZ) {
   const MGWIZ *z   = vmg_wizard_mgwiz(mg->mg_ptr);
   IV           zid = vmg_wizard_id(mg->mg_ptr);
   if (zid == wid) {
#if VMG_UVAR
    /* If the current has no uvar, short-circuit uvar deletion. */
    uvars = z->uvar ? (uvars + 1) : 0;
#endif /* VMG_UVAR */
    break;
#if VMG_UVAR
   } else if (z->uvar) {
    ++uvars;
    /* We can't break here since we need to find the ext magic to delete. */
#endif /* VMG_UVAR */
   }
  }
 }
 if (!mg)
  return 0;

 if (prevmagic) {
  prevmagic->mg_moremagic = moremagic;
 } else {
  SvMAGIC_set(sv, moremagic);
 }
 mg->mg_moremagic = NULL;

 /* Destroy private data */
 if (mg->mg_obj != sv)
  SvREFCNT_dec(mg->mg_obj);
 /* Unreference the wizard */
 SvREFCNT_dec((SV *) mg->mg_ptr);
 Safefree(mg);

#if VMG_UVAR
 if (uvars == 1 && SvTYPE(sv) >= SVt_PVHV) {
  /* mg was the first ext magic in the chain that had uvar */

  for (mg = moremagic; mg; mg = mg->mg_moremagic) {
   if (mg->mg_type == PERL_MAGIC_ext && mg->mg_private == SIG_WIZ) {
    const MGWIZ *z = vmg_wizard_mgwiz(mg->mg_ptr);
    if (z->uvar) {
     ++uvars;
     break;
    }
   }
  }

  if (uvars == 1) {
   struct ufuncs *uf;
   for (prevmagic = NULL, mg = SvMAGIC(sv); mg; prevmagic = mg, mg = moremagic){
    moremagic = mg->mg_moremagic;
    if (mg->mg_type == PERL_MAGIC_uvar)
     break;
   }
   /* assert(mg); */
   uf = (struct ufuncs *) mg->mg_ptr;
   /* assert(uf->uf_val == vmg_svt_val); */
   if (uf[1].uf_val || uf[1].uf_set) {
    /* Revert the original uvar magic. */
    uf[0] = uf[1];
    Renew(uf, 1, struct ufuncs);
    mg->mg_ptr = (char *) uf;
    mg->mg_len = sizeof(struct ufuncs);
   } else {
    /* Remove the uvar magic. */
    vmg_uvar_del(sv, prevmagic, mg, moremagic);
   }
  }
 }
#endif /* VMG_UVAR */

 return 1;
}

/* ... OP info ............................................................. */

#define VMG_OP_INFO_NAME   1
#define VMG_OP_INFO_OBJECT 2

#if VMG_THREADSAFE
STATIC perl_mutex vmg_op_name_init_mutex;
#endif

STATIC U32           vmg_op_name_init      = 0;
STATIC unsigned char vmg_op_name_len[MAXO] = { 0 };

STATIC void vmg_op_info_init(pTHX_ unsigned int opinfo) {
#define vmg_op_info_init(W) vmg_op_info_init(aTHX_ (W))
 switch (opinfo) {
  case VMG_OP_INFO_NAME:
#if VMG_THREADSAFE
   MUTEX_LOCK(&vmg_op_name_init_mutex);
#endif
   if (!vmg_op_name_init) {
    OPCODE t;
    for (t = 0; t < OP_max; ++t)
     vmg_op_name_len[t] = strlen(PL_op_name[t]);
    vmg_op_name_init = 1;
   }
#if VMG_THREADSAFE
   MUTEX_UNLOCK(&vmg_op_name_init_mutex);
#endif
   break;
  case VMG_OP_INFO_OBJECT: {
   dMY_CXT;
   if (!MY_CXT.b__op_stashes[0]) {
    opclass c;
    require_pv("B.pm");
    for (c = OPc_NULL; c < OPc_MAX; ++c)
     MY_CXT.b__op_stashes[c] = gv_stashpv(vmg_opclassnames[c], 1);
   }
   break;
  }
  default:
   break;
 }
}

STATIC SV *vmg_op_info(pTHX_ unsigned int opinfo) {
#define vmg_op_info(W) vmg_op_info(aTHX_ (W))
 if (!PL_op)
  return &PL_sv_undef;

 switch (opinfo) {
  case VMG_OP_INFO_NAME: {
   OPCODE t = PL_op->op_type;
   return sv_2mortal(newSVpvn(PL_op_name[t], vmg_op_name_len[t]));
  }
  case VMG_OP_INFO_OBJECT: {
   dMY_CXT;
   return sv_bless(sv_2mortal(newRV_noinc(newSViv(PTR2IV(PL_op)))),
                   MY_CXT.b__op_stashes[vmg_opclass(PL_op)]);
  }
  default:
   break;
 }

 return &PL_sv_undef;
}

/* ... svt callbacks ....................................................... */

#define VMG_CB_CALL_ARGS_MASK  15
#define VMG_CB_CALL_ARGS_SHIFT 4
#define VMG_CB_CALL_OPINFO     (VMG_OP_INFO_NAME|VMG_OP_INFO_OBJECT)

STATIC int vmg_cb_call(pTHX_ SV *cb, unsigned int flags, SV *sv, ...) {
 va_list ap;
 int ret = 0;
 unsigned int i, args, opinfo;
 SV *svr;

 dSP;

 args    = flags & VMG_CB_CALL_ARGS_MASK;
 flags >>= VMG_CB_CALL_ARGS_SHIFT;
 opinfo  = flags & VMG_CB_CALL_OPINFO;

 ENTER;
 SAVETMPS;

 PUSHMARK(SP);
 EXTEND(SP, args + 1);
 PUSHs(sv_2mortal(newRV_inc(sv)));
 va_start(ap, sv);
 for (i = 0; i < args; ++i) {
  SV *sva = va_arg(ap, SV *);
  PUSHs(sva ? sva : &PL_sv_undef);
 }
 va_end(ap);
 if (opinfo)
  XPUSHs(vmg_op_info(opinfo));
 PUTBACK;

 vmg_call_sv(cb, G_SCALAR, 0);

 SPAGAIN;
 svr = POPs;
 if (SvOK(svr))
  ret = (int) SvIV(svr);
 PUTBACK;

 FREETMPS;
 LEAVE;

 return ret;
}

#define VMG_CB_FLAGS(OI, A) \
        ((((unsigned int) (OI)) << VMG_CB_CALL_ARGS_SHIFT) | (A))

#define vmg_cb_call1(I, OI, S, A1) \
        vmg_cb_call(aTHX_ (I), VMG_CB_FLAGS((OI), 1), (S), (A1))
#define vmg_cb_call2(I, OI, S, A1, A2) \
        vmg_cb_call(aTHX_ (I), VMG_CB_FLAGS((OI), 2), (S), (A1), (A2))
#define vmg_cb_call3(I, OI, S, A1, A2, A3) \
        vmg_cb_call(aTHX_ (I), VMG_CB_FLAGS((OI), 3), (S), (A1), (A2), (A3))

STATIC int vmg_svt_get(pTHX_ SV *sv, MAGIC *mg) {
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 return vmg_cb_call1(w->cb_get, w->opinfo, sv, mg->mg_obj);
}

STATIC int vmg_svt_set(pTHX_ SV *sv, MAGIC *mg) {
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 return vmg_cb_call1(w->cb_set, w->opinfo, sv, mg->mg_obj);
}

STATIC U32 vmg_svt_len(pTHX_ SV *sv, MAGIC *mg) {
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 unsigned int opinfo = w->opinfo;
 U32 len, ret;
 SV *svr;
 svtype t = SvTYPE(sv);

 dSP;

 ENTER;
 SAVETMPS;

 PUSHMARK(SP);
 EXTEND(SP, 3);
 PUSHs(sv_2mortal(newRV_inc(sv)));
 PUSHs(mg->mg_obj ? mg->mg_obj : &PL_sv_undef);
 if (t < SVt_PVAV) {
  STRLEN l;
#if VMG_HAS_PERL(5, 9, 3)
  const U8 *s = SvPV_const(sv, l);
#else
  U8 *s = SvPV(sv, l);
#endif
  if (DO_UTF8(sv))
   len = utf8_length(s, s + l);
  else
   len = l;
  mPUSHu(len);
 } else if (t == SVt_PVAV) {
  len = av_len((AV *) sv) + 1;
  mPUSHu(len);
 } else {
  len = 0;
  PUSHs(&PL_sv_undef);
 }
 if (opinfo)
  XPUSHs(vmg_op_info(opinfo));
 PUTBACK;

 vmg_call_sv(w->cb_len, G_SCALAR, 0);

 SPAGAIN;
 svr = POPs;
 ret = SvOK(svr) ? (U32) SvUV(svr) : len;
 if (t == SVt_PVAV)
  --ret;
 PUTBACK;

 FREETMPS;
 LEAVE;

 return ret;
}

STATIC int vmg_svt_clear(pTHX_ SV *sv, MAGIC *mg) {
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 return vmg_cb_call1(w->cb_clear, w->opinfo, sv, mg->mg_obj);
}

STATIC int vmg_svt_free(pTHX_ SV *sv, MAGIC *mg) {
 const MGWIZ *w;
 int ret = 0;
 SV *svr;

 dSP;

 /* Don't even bother if we are in global destruction - the wizard is prisoner
  * of circular references and we are way beyond user realm */
 if (PL_dirty)
  return 0;

 w = vmg_wizard_mgwiz(mg->mg_ptr);

 /* So that it survives the temp cleanup below */
 SvREFCNT_inc_simple_void(sv);

#if !(VMG_HAS_PERL_MAINT(5, 11, 0, 32686) || VMG_HAS_PERL(5, 12, 0))
 /* The previous magic tokens were freed but the magic chain wasn't updated, so
  * if you access the sv from the callback the old deleted magics will trigger
  * and cause memory misreads. Change 32686 solved it that way : */
 SvMAGIC_set(sv, mg);
#endif

 ENTER;
 SAVETMPS;

 PUSHMARK(SP);
 EXTEND(SP, 2);
 PUSHs(sv_2mortal(newRV_inc(sv)));
 PUSHs(mg->mg_obj ? mg->mg_obj : &PL_sv_undef);
 if (w->opinfo)
  XPUSHs(vmg_op_info(w->opinfo));
 PUTBACK;

 vmg_call_sv(w->cb_free, G_SCALAR, 1);

 SPAGAIN;
 svr = POPs;
 if (SvOK(svr))
  ret = (int) SvIV(svr);
 PUTBACK;

 FREETMPS;
 LEAVE;

 /* Calling SvREFCNT_dec() will trigger destructors in an infinite loop, so
  * we have to rely on SvREFCNT() being a lvalue. Heck, even the core does it */
 --SvREFCNT(sv);

 /* Perl_mg_free will get rid of the magic and decrement mg->mg_obj and
  * mg->mg_ptr reference count */
 return ret;
}

#if MGf_COPY
STATIC int vmg_svt_copy(pTHX_ SV *sv, MAGIC *mg, SV *nsv, const char *key,
# if VMG_HAS_PERL_MAINT(5, 11, 0, 33256) || VMG_HAS_PERL(5, 12, 0)
  I32 keylen
# else
  int keylen
# endif
 ) {
 SV *keysv;
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 int ret;

 if (keylen == HEf_SVKEY) {
  keysv = (SV *) key;
 } else {
  keysv = newSVpvn(key, keylen);
 }

 ret = vmg_cb_call3(w->cb_copy, w->opinfo, sv, mg->mg_obj, keysv, nsv);

 if (keylen != HEf_SVKEY) {
  SvREFCNT_dec(keysv);
 }

 return ret;
}
#endif /* MGf_COPY */

#if 0 /*  MGf_DUP */
STATIC int vmg_svt_dup(pTHX_ MAGIC *mg, CLONE_PARAMS *param) {
 return 0;
}
#endif /* MGf_DUP */

#if MGf_LOCAL
STATIC int vmg_svt_local(pTHX_ SV *nsv, MAGIC *mg) {
 const MGWIZ *w = vmg_wizard_mgwiz(mg->mg_ptr);
 return vmg_cb_call1(w->cb_local, w->opinfo, nsv, mg->mg_obj);
}
#endif /* MGf_LOCAL */

#if VMG_UVAR
STATIC OP *vmg_pp_resetuvar(pTHX) {
 SvRMAGICAL_on(cSVOP_sv);
 return NORMAL;
}

STATIC I32 vmg_svt_val(pTHX_ IV action, SV *sv) {
 struct ufuncs *uf;
 MAGIC *mg, *umg;
 SV *key = NULL, *newkey = NULL;
 int tied = 0;

 umg = mg_find(sv, PERL_MAGIC_uvar);
 /* umg can't be NULL or we wouldn't be there. */
 key = umg->mg_obj;
 uf  = (struct ufuncs *) umg->mg_ptr;

 if (uf[1].uf_val)
  uf[1].uf_val(aTHX_ action, sv);
 if (uf[1].uf_set)
  uf[1].uf_set(aTHX_ action, sv);

 action &= HV_FETCH_ISSTORE | HV_FETCH_ISEXISTS | HV_FETCH_LVALUE | HV_DELETE;
 for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
  const MGWIZ *w;
  switch (mg->mg_type) {
   case PERL_MAGIC_ext:
    break;
   case PERL_MAGIC_tied:
    ++tied;
    continue;
   default:
    continue;
  }
  if (mg->mg_private != SIG_WIZ) continue;
  w = vmg_wizard_mgwiz(mg->mg_ptr);
  switch (w->uvar) {
   case 0:
    continue;
   case 2:
    if (!newkey)
     newkey = key = umg->mg_obj = sv_mortalcopy(umg->mg_obj);
  }
  switch (action) {
   case 0:
    if (w->cb_fetch)
     vmg_cb_call2(w->cb_fetch, w->opinfo, sv, mg->mg_obj, key);
    break;
   case HV_FETCH_ISSTORE:
   case HV_FETCH_LVALUE:
   case (HV_FETCH_ISSTORE|HV_FETCH_LVALUE):
    if (w->cb_store)
     vmg_cb_call2(w->cb_store, w->opinfo, sv, mg->mg_obj, key);
    break;
   case HV_FETCH_ISEXISTS:
    if (w->cb_exists)
     vmg_cb_call2(w->cb_exists, w->opinfo, sv, mg->mg_obj, key);
    break;
   case HV_DELETE:
    if (w->cb_delete)
     vmg_cb_call2(w->cb_delete, w->opinfo, sv, mg->mg_obj, key);
    break;
  }
 }

 if (SvRMAGICAL(sv) && !tied) {
  /* Temporarily hide the RMAGICAL flag of the hash so it isn't wrongly
   * mistaken for a tied hash by the rest of hv_common. It will be reset by
   * the op_ppaddr of a new fake op injected between the current and the next
   * one. */
  OP *o = PL_op;
  if (!o->op_next || o->op_next->op_ppaddr != vmg_pp_resetuvar) {
   SVOP *svop;
   NewOp(1101, svop, 1, SVOP);
   svop->op_type   = OP_STUB;
   svop->op_ppaddr = vmg_pp_resetuvar;
   svop->op_next   = o->op_next;
   svop->op_flags  = 0;
   svop->op_sv     = sv;
   o->op_next      = (OP *) svop;
  }
  SvRMAGICAL_off(sv);
 }

 return 0;
}
#endif /* VMG_UVAR */

/* --- Macros for the XS section ------------------------------------------- */

#define VMG_SET_CB(S, N)              \
 cb = (S);                            \
 w->cb_ ## N = (SvOK(cb) && SvROK(cb)) ? SvREFCNT_inc(SvRV(cb)) : NULL;

#define VMG_SET_SVT_CB(S, N)          \
 cb = (S);                            \
 if (SvOK(cb) && SvROK(cb)) {         \
  t->svt_ ## N = vmg_svt_ ## N;       \
  w->cb_  ## N = SvREFCNT_inc(SvRV(cb)); \
 } else {                             \
  t->svt_ ## N = NULL;                \
  w->cb_  ## N = NULL;                \
 }

#if VMG_THREADSAFE

STATIC void vmg_cleanup(pTHX_ void *ud) {
 dMY_CXT;

 ptable_free(MY_CXT.wizards);
 MY_CXT.wizards = NULL;
}

#endif /* VMG_THREADSAFE */

/* --- XS ------------------------------------------------------------------ */

MODULE = Variable::Magic            PACKAGE = Variable::Magic

PROTOTYPES: ENABLE

BOOT:
{
 HV *stash;

 MY_CXT_INIT;
#if VMG_THREADSAFE
 MY_CXT.wizards = ptable_new();
 MY_CXT.owner   = aTHX;
#endif
 MY_CXT.b__op_stashes[0] = NULL;
#if VMG_THREADSAFE
 MUTEX_INIT(&vmg_op_name_init_mutex);
 call_atexit(vmg_cleanup, NULL);
#endif

 stash = gv_stashpv(__PACKAGE__, 1);
 newCONSTSUB(stash, "MGf_COPY",  newSVuv(MGf_COPY));
 newCONSTSUB(stash, "MGf_DUP",   newSVuv(MGf_DUP));
 newCONSTSUB(stash, "MGf_LOCAL", newSVuv(MGf_LOCAL));
 newCONSTSUB(stash, "VMG_UVAR",  newSVuv(VMG_UVAR));
 newCONSTSUB(stash, "VMG_COMPAT_ARRAY_PUSH_NOLEN",
                    newSVuv(VMG_COMPAT_ARRAY_PUSH_NOLEN));
 newCONSTSUB(stash, "VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID",
                    newSVuv(VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID));
 newCONSTSUB(stash, "VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID",
                    newSVuv(VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID));
 newCONSTSUB(stash, "VMG_COMPAT_ARRAY_UNDEF_CLEAR",
                    newSVuv(VMG_COMPAT_ARRAY_UNDEF_CLEAR));
 newCONSTSUB(stash, "VMG_COMPAT_SCALAR_LENGTH_NOLEN",
                    newSVuv(VMG_COMPAT_SCALAR_LENGTH_NOLEN));
 newCONSTSUB(stash, "VMG_COMPAT_GLOB_GET", newSVuv(VMG_COMPAT_GLOB_GET));
 newCONSTSUB(stash, "VMG_PERL_PATCHLEVEL", newSVuv(VMG_PERL_PATCHLEVEL));
 newCONSTSUB(stash, "VMG_THREADSAFE",      newSVuv(VMG_THREADSAFE));
 newCONSTSUB(stash, "VMG_FORKSAFE",        newSVuv(VMG_FORKSAFE));
 newCONSTSUB(stash, "VMG_OP_INFO_NAME",    newSVuv(VMG_OP_INFO_NAME));
 newCONSTSUB(stash, "VMG_OP_INFO_OBJECT",  newSVuv(VMG_OP_INFO_OBJECT));
}

#if VMG_THREADSAFE

void
CLONE(...)
PROTOTYPE: DISABLE
PREINIT:
 ptable *t;
 U32     had_b__op_stash = 0;
 opclass c;
PPCODE:
 {
  my_cxt_t ud;
  dMY_CXT;

  ud.wizards = t = ptable_new();
  ud.owner   = MY_CXT.owner;
  ptable_walk(MY_CXT.wizards, vmg_ptable_clone, &ud);

  for (c = OPc_NULL; c < OPc_MAX; ++c) {
   if (MY_CXT.b__op_stashes[c])
    had_b__op_stash |= (((U32) 1) << c);
  }
 }
 {
  MY_CXT_CLONE;
  MY_CXT.wizards = t;
  MY_CXT.owner   = aTHX;
  for (c = OPc_NULL; c < OPc_MAX; ++c) {
   MY_CXT.b__op_stashes[c] = (had_b__op_stash & (((U32) 1) << c))
                              ? gv_stashpv(vmg_opclassnames[c], 1) : NULL;
  }
 }
 XSRETURN(0);

#endif /* VMG_THREADSAFE */

SV *_wizard(...)
PROTOTYPE: DISABLE
PREINIT:
 I32 i = 0;
 UV opinfo;
 MGWIZ *w;
 MGVTBL *t;
 SV *cb;
CODE:
 dMY_CXT;

 if (items != 7
#if MGf_COPY
              + 1
#endif /* MGf_COPY */
#if MGf_DUP
              + 1
#endif /* MGf_DUP */
#if MGf_LOCAL
              + 1
#endif /* MGf_LOCAL */
#if VMG_UVAR
              + 5
#endif /* VMG_UVAR */
              ) { croak(vmg_wrongargnum); }

 Newx(t, 1, MGVTBL);
 Newx(w, 1, MGWIZ);

 VMG_SET_CB(ST(i++), data);

 cb = ST(i++);
 opinfo = SvOK(cb) ? SvUV(cb) : 0;
 w->opinfo = (U8) ((opinfo < 255) ? opinfo : 255);
 if (w->opinfo)
  vmg_op_info_init(w->opinfo);

 VMG_SET_SVT_CB(ST(i++), get);
 VMG_SET_SVT_CB(ST(i++), set);
 VMG_SET_SVT_CB(ST(i++), len);
 VMG_SET_SVT_CB(ST(i++), clear);
 VMG_SET_SVT_CB(ST(i++), free);
#if MGf_COPY
 VMG_SET_SVT_CB(ST(i++), copy);
#endif /* MGf_COPY */
#if MGf_DUP
 /* VMG_SET_SVT_CB(ST(i++), dup); */
 i++;
 t->svt_dup = NULL;
 w->cb_dup  = NULL;
#endif /* MGf_DUP */
#if MGf_LOCAL
 VMG_SET_SVT_CB(ST(i++), local);
#endif /* MGf_LOCAL */
#if VMG_UVAR
 VMG_SET_CB(ST(i++), fetch);
 VMG_SET_CB(ST(i++), store);
 VMG_SET_CB(ST(i++), exists);
 VMG_SET_CB(ST(i++), delete);
 cb = ST(i++);
 if (w->cb_fetch || w->cb_store || w->cb_exists || w->cb_delete)
  w->uvar = SvTRUE(cb) ? 2 : 1;
 else
  w->uvar = 0;
#endif /* VMG_UVAR */
#if VMG_MULTIPLICITY
 w->owner = aTHX;
#endif /* VMG_MULTIPLICITY */
 w->vtbl  = t;
#if VMG_THREADSAFE
 ptable_store(MY_CXT.wizards, w, w);
#endif /* VMG_THREADSAFE */

 RETVAL = newRV_noinc(vmg_wizard_new(w));
OUTPUT:
 RETVAL

SV *cast(SV *sv, SV *wiz, ...)
PROTOTYPE: \[$@%&*]$@
PREINIT:
 SV **args = NULL;
 I32 i = 0;
CODE:
 if (items > 2) {
  i = items - 2;
  args = &ST(2);
 }
 RETVAL = newSVuv(vmg_cast(SvRV(sv), vmg_wizard_validate(wiz), args, i));
OUTPUT:
 RETVAL

void
getdata(SV *sv, SV *wiz)
PROTOTYPE: \[$@%&*]$
PREINIT:
 SV *data;
PPCODE:
 data = vmg_data_get(SvRV(sv), vmg_wizard_validate(wiz));
 if (!data)
  XSRETURN_EMPTY;
 ST(0) = data;
 XSRETURN(1);

SV *dispell(SV *sv, SV *wiz)
PROTOTYPE: \[$@%&*]$
CODE:
 RETVAL = newSVuv(vmg_dispell(SvRV(sv), vmg_wizard_validate(wiz)));
OUTPUT:
 RETVAL
