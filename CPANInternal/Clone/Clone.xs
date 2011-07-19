#include <assert.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static char *rcs_id = "$Id: Clone.xs,v 0.31 2009/01/20 04:54:37 ray Exp $";

#define CLONE_KEY(x) ((char *) &x) 

#define CLONE_STORE(x,y)						\
do {									\
    if (!hv_store(hseen, CLONE_KEY(x), PTRSIZE, SvREFCNT_inc(y), 0)) {	\
	SvREFCNT_dec(y); /* Restore the refcount */			\
	croak("Can't store clone in seen hash (hseen)");		\
    }									\
    else {	\
  TRACEME(("storing ref = 0x%x clone = 0x%x\n", ref, clone));	\
  TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));	\
  TRACEME(("ref = 0x%x(%d)\n", ref, SvREFCNT(ref)));	\
    }									\
} while (0)

#define CLONE_FETCH(x) (hv_fetch(hseen, CLONE_KEY(x), PTRSIZE, 0))

static SV *hv_clone (SV *, SV *, HV *, int);
static SV *av_clone (SV *, SV *, HV *, int);
static SV *sv_clone (SV *, HV *, int);
static SV *rv_clone (SV *, HV *, int);

#ifdef DEBUG_CLONE
#define TRACEME(a) printf("%s:%d: ",__FUNCTION__, __LINE__) && printf a;
#else
#define TRACEME(a)
#endif

static SV *
hv_clone (SV * ref, SV * target, HV* hseen, int depth)
{
  HV *clone = (HV *) target;
  HV *self = (HV *) ref;
  HE *next = NULL;
  int recur = depth ? depth - 1 : 0;

  assert(SvTYPE(ref) == SVt_PVHV);

  TRACEME(("ref = 0x%x(%d)\n", ref, SvREFCNT(ref)));

  hv_iterinit (self);
  while (next = hv_iternext (self))
    {
      SV *key = hv_iterkeysv (next);
      TRACEME(("clone item %s\n", SvPV_nolen(key) ));
      hv_store_ent (clone, key, 
                sv_clone (hv_iterval (self, next), hseen, recur), 0);
    }

  TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));
  return (SV *) clone;
}

static SV *
av_clone (SV * ref, SV * target, HV* hseen, int depth)
{
  AV *clone = (AV *) target;
  AV *self = (AV *) ref;
  SV **svp;
  SV *val = NULL;
  I32 arrlen = 0;
  int i = 0;
  int recur = depth ? depth - 1 : 0;

  assert(SvTYPE(ref) == SVt_PVAV);

  TRACEME(("ref = 0x%x(%d)\n", ref, SvREFCNT(ref)));

  /* The following is a holdover from a very old version */
  /* possible cause of memory leaks */
  /* if ( (SvREFCNT(ref) > 1) ) */
  /*   CLONE_STORE(ref, (SV *)clone); */

  arrlen = av_len (self);
  av_extend (clone, arrlen);

  for (i = 0; i <= arrlen; i++)
    {
      svp = av_fetch (self, i, 0);
      if (svp)
	av_store (clone, i, sv_clone (*svp, hseen, recur));
    }

  TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));
  return (SV *) clone;
}

static SV *
rv_clone (SV * ref, HV* hseen, int depth)
{
  SV *clone = NULL;
  SV *rv = NULL;

  assert(SvROK(ref));

  TRACEME(("ref = 0x%x(%d)\n", ref, SvREFCNT(ref)));

  if (!SvROK (ref))
    return NULL;

  if (sv_isobject (ref))
    {
      clone = newRV_noinc(sv_clone (SvRV(ref), hseen, depth));
      sv_2mortal (sv_bless (clone, SvSTASH (SvRV (ref))));
    }
  else
    clone = newRV_inc(sv_clone (SvRV(ref), hseen, depth));
    
  TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));
  return clone;
}

static SV *
sv_clone (SV * ref, HV* hseen, int depth)
{
  SV *clone = ref;
  SV **seen = NULL;
#if PERL_REVISION >= 5 && PERL_VERSION > 8
  /* This is a hack for perl 5.9.*, save everything */
  /* until I find out why mg_find is no longer working */
  UV visible = 1;
#else
  UV visible = (SvREFCNT(ref) > 1) || (SvMAGICAL(ref) && mg_find(ref, '<'));
#endif
  int magic_ref = 0;

  TRACEME(("ref = 0x%x(%d)\n", ref, SvREFCNT(ref)));

  if (depth == 0)
    return SvREFCNT_inc(ref);

  if (visible && (seen = CLONE_FETCH(ref)))
    {
      TRACEME(("fetch ref (0x%x)\n", ref));
      return SvREFCNT_inc(*seen);
    }

  TRACEME(("switch: (0x%x)\n", ref));
  switch (SvTYPE (ref))
    {
      case SVt_NULL:	/* 0 */
        TRACEME(("sv_null\n"));
        clone = newSVsv (ref);
        break;
      case SVt_IV:		/* 1 */
        TRACEME(("int scalar\n"));
      case SVt_NV:		/* 2 */
        TRACEME(("double scalar\n"));
        clone = newSVsv (ref);
        break;
#if PERL_VERSION <= 10
      case SVt_RV:		/* 3 */
        TRACEME(("ref scalar\n"));
        clone = newSVsv (ref);
        break;
#endif
      case SVt_PV:		/* 4 */
        TRACEME(("string scalar\n"));
        clone = newSVsv (ref);
        break;
      case SVt_PVIV:		/* 5 */
        TRACEME (("PVIV double-type\n"));
      case SVt_PVNV:		/* 6 */
        TRACEME (("PVNV double-type\n"));
        clone = newSVsv (ref);
        break;
      case SVt_PVMG:	/* 7 */
        TRACEME(("magic scalar\n"));
        clone = newSVsv (ref);
        break;
      case SVt_PVAV:	/* 10 */
        clone = (SV *) newAV();
        break;
      case SVt_PVHV:	/* 11 */
        clone = (SV *) newHV();
        break;
      #if PERL_VERSION <= 8
      case SVt_PVBM:	/* 8 */
      #elif PERL_VERSION >= 11
      case SVt_REGEXP:	/* 8 */
      #endif
      case SVt_PVLV:	/* 9 */
      case SVt_PVCV:	/* 12 */
      case SVt_PVGV:	/* 13 */
      case SVt_PVFM:	/* 14 */
      case SVt_PVIO:	/* 15 */
        TRACEME(("default: type = 0x%x\n", SvTYPE (ref)));
        clone = SvREFCNT_inc(ref);  /* just return the ref */
        break;
      default:
        croak("unkown type: 0x%x", SvTYPE(ref));
    }

  /**
    * It is *vital* that this is performed *before* recursion,
    * to properly handle circular references. cb 2001-02-06
    */

  if ( visible )
    CLONE_STORE(ref,clone);

    /*
     * We'll assume (in the absence of evidence to the contrary) that A) a
     * tied hash/array doesn't store its elements in the usual way (i.e.
     * the mg->mg_object(s) take full responsibility for them) and B) that
     * references aren't tied.
     *
     * If theses assumptions hold, the three options below are mutually
     * exclusive.
     *
     * More precisely: 1 & 2 are probably mutually exclusive; 2 & 3 are 
     * definitely mutually exclusive; we have to test 1 before giving 2
     * a chance; and we'll assume that 1 & 3 are mutually exclusive unless
     * and until we can be test-cased out of our delusion.
     *
     * chocolateboy: 2001-05-29
     */
     
    /* 1: TIED */
  if (SvMAGICAL(ref) )
    {
      MAGIC* mg;
      MGVTBL *vtable = 0;

      for (mg = SvMAGIC(ref); mg; mg = mg->mg_moremagic) 
      {
        SV *obj = (SV *) NULL;
	/* we don't want to clone a qr (regexp) object */
	/* there are probably other types as well ...  */
        TRACEME(("magic type: %c\n", mg->mg_type));
        /* Some mg_obj's can be null, don't bother cloning */
        if ( mg->mg_obj != NULL )
        {
          switch (mg->mg_type)
          {
            case 'r':	/* PERL_MAGIC_qr  */
              obj = mg->mg_obj; 
              break;
            case 't':	/* PERL_MAGIC_taint */
	      continue;
              break;
            case '<':	/* PERL_MAGIC_backref */
	      continue;
              break;
            case '@':  /* PERL_MAGIC_arylen_p */
             continue;
              break;
            default:
              obj = sv_clone(mg->mg_obj, hseen, -1); 
          }
        } else {
          TRACEME(("magic object for type %c in NULL\n", mg->mg_type));
        }
	magic_ref++;
	/* this is plain old magic, so do the same thing */
        sv_magic(clone, 
                 obj,
                 mg->mg_type, 
                 mg->mg_ptr, 
                 mg->mg_len);
      }
      /* major kludge - why does the vtable for a qr type need to be null? */
      if ( mg = mg_find(clone, 'r') )
        mg->mg_virtual = (MGVTBL *) NULL;
    }
    /* 2: HASH/ARRAY  - (with 'internal' elements) */
  if ( magic_ref )
  {
    ;;
  }
  else if ( SvTYPE(ref) == SVt_PVHV )
    clone = hv_clone (ref, clone, hseen, depth);
  else if ( SvTYPE(ref) == SVt_PVAV )
    clone = av_clone (ref, clone, hseen, depth);
    /* 3: REFERENCE (inlined for speed) */
  else if (SvROK (ref))
    {
      TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));
      SvREFCNT_dec(SvRV(clone));
      SvRV(clone) = sv_clone (SvRV(ref), hseen, depth); /* Clone the referent */
      if (sv_isobject (ref))
      {
          sv_bless (clone, SvSTASH (SvRV (ref)));
      }
      if (SvWEAKREF(ref)) {
          sv_rvweaken(clone);
      }
    }

  TRACEME(("clone = 0x%x(%d)\n", clone, SvREFCNT(clone)));
  return clone;
}

MODULE = Clone		PACKAGE = Clone		

PROTOTYPES: ENABLE

void
clone(self, depth=-1)
	SV *self
	int depth
	PREINIT:
	SV *clone = &PL_sv_undef;
        HV *hseen = newHV();
	PPCODE:
	TRACEME(("ref = 0x%x\n", self));
	clone = sv_clone(self, hseen, depth);
	hv_clear(hseen);  /* Free HV */
        SvREFCNT_dec((SV *)hseen);
	EXTEND(SP,1);
	PUSHs(sv_2mortal(clone));
