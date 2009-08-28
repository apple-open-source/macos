
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* *********** ppport stuff */

#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(x) ((void)x)
#endif

#if defined(PERL_GCC_PEDANTIC)
#  ifndef PERL_GCC_BRACE_GROUPS_FORBIDDEN
#    define PERL_GCC_BRACE_GROUPS_FORBIDDEN
#  endif
#endif

#if defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN) && !defined(__cplusplus)
#  ifndef PERL_USE_GCC_BRACE_GROUPS
#    define PERL_USE_GCC_BRACE_GROUPS
#  endif
#endif

#ifndef SvREFCNT_inc
#  ifdef PERL_USE_GCC_BRACE_GROUPS
#    define SvREFCNT_inc(sv)		\
      ({				\
          SV * const _sv = (SV*)(sv);	\
          if (_sv)			\
               (SvREFCNT(_sv))++;	\
          _sv;				\
      })
#  else
#    define SvREFCNT_inc(sv)	\
          ((PL_Sv=(SV*)(sv)) ? (++(SvREFCNT(PL_Sv)),PL_Sv) : NULL)
#  endif
#endif

#ifndef dAX
#  define dAX                            I32 ax = MARK - PL_stack_base + 1
#endif

#ifndef dVAR
#  define dVAR                           dNOOP
#endif

#ifndef packWARN
#  define packWARN(a)                    (a)
#endif

/* *********** end ppport.h stuff */

/* Most of this code is backported from the bleadperl patch's
   mro.c, and then modified to work with Class::C3's
   internals.
*/

AV*
__mro_linear_isa_c3(pTHX_ HV* stash, HV* cache, I32 level)
{
    AV* retval;
    GV** gvp;
    GV* gv;
    AV* isa;
    const char* stashname;
    STRLEN stashname_len;
    I32 made_mortal_cache = 0;

    assert(stash);

    stashname = HvNAME(stash);
    stashname_len = strlen(stashname);
    if (!stashname)
      Perl_croak(aTHX_
                 "Can't linearize anonymous symbol table");

    if (level > 100)
        Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
              stashname);

    if(!cache) {
        cache = (HV*)sv_2mortal((SV*)newHV());
        made_mortal_cache = 1;
    }
    else {
        SV** cache_entry = hv_fetch(cache, stashname, stashname_len, 0);
        if(cache_entry)
            return (AV*)SvREFCNT_inc(*cache_entry);
    }

    /* not in cache, make a new one */

    gvp = (GV**)hv_fetch(stash, "ISA", 3, FALSE);
    isa = (gvp && (gv = *gvp) && gv != (GV*)&PL_sv_undef) ? GvAV(gv) : NULL;
    if(isa && AvFILLp(isa) >= 0) {
        SV** seqs_ptr;
        I32 seqs_items;
        HV* const tails = (HV*)sv_2mortal((SV*)newHV());
        AV* const seqs = (AV*)sv_2mortal((SV*)newAV());
        I32* heads;

        /* This builds @seqs, which is an array of arrays.
           The members of @seqs are the MROs of
           the members of @ISA, followed by @ISA itself.
        */
        I32 items = AvFILLp(isa) + 1;
        SV** isa_ptr = AvARRAY(isa);
        while(items--) {
            SV* const isa_item = *isa_ptr++;
            HV* const isa_item_stash = gv_stashsv(isa_item, 0);
            if(!isa_item_stash) {
                /* if no stash, make a temporary fake MRO
                   containing just itself */
                AV* const isa_lin = newAV();
                av_push(isa_lin, newSVsv(isa_item));
                av_push(seqs, (SV*)isa_lin);
            }
            else {
                /* recursion */
                AV* const isa_lin = __mro_linear_isa_c3(aTHX_ isa_item_stash, cache, level + 1);
                av_push(seqs, (SV*)isa_lin);
            }
        }
        av_push(seqs, SvREFCNT_inc((SV*)isa));

        /* This builds "heads", which as an array of integer array
           indices, one per seq, which point at the virtual "head"
           of the seq (initially zero) */
        Newz(0xdead, heads, AvFILLp(seqs)+1, I32);

        /* This builds %tails, which has one key for every class
           mentioned in the tail of any sequence in @seqs (tail meaning
           everything after the first class, the "head").  The value
           is how many times this key appears in the tails of @seqs.
        */
        seqs_ptr = AvARRAY(seqs);
        seqs_items = AvFILLp(seqs) + 1;
        while(seqs_items--) {
            AV* const seq = (AV*)*seqs_ptr++;
            I32 seq_items = AvFILLp(seq);
            if(seq_items > 0) {
                SV** seq_ptr = AvARRAY(seq) + 1;
                while(seq_items--) {
                    SV* const seqitem = *seq_ptr++;
                    HE* const he = hv_fetch_ent(tails, seqitem, 0, 0);
                    if(!he) {
                        hv_store_ent(tails, seqitem, newSViv(1), 0);
                    }
                    else {
                        SV* const val = HeVAL(he);
                        sv_inc(val);
                    }
                }
            }
        }

        /* Initialize retval to build the return value in */
        retval = newAV();
        av_push(retval, newSVpvn(stashname, stashname_len)); /* us first */

        /* This loop won't terminate until we either finish building
           the MRO, or get an exception. */
        while(1) {
            SV* cand = NULL;
            SV* winner = NULL;
            int s;

            /* "foreach $seq (@seqs)" */
            SV** const avptr = AvARRAY(seqs);
            for(s = 0; s <= AvFILLp(seqs); s++) {
                SV** svp;
                AV * const seq = (AV*)(avptr[s]);
                SV* seqhead;
                if(!seq) continue; /* skip empty seqs */
                svp = av_fetch(seq, heads[s], 0);
                seqhead = *svp; /* seqhead = head of this seq */
                if(!winner) {
                    HE* tail_entry;
                    SV* val;
                    /* if we haven't found a winner for this round yet,
                       and this seqhead is not in tails (or the count
                       for it in tails has dropped to zero), then this
                       seqhead is our new winner, and is added to the
                       final MRO immediately */
                    cand = seqhead;
                    if((tail_entry = hv_fetch_ent(tails, cand, 0, 0))
                       && (val = HeVAL(tail_entry))
                       && (SvIVX(val) > 0))
                           continue;
                    winner = newSVsv(cand);
                    av_push(retval, winner);
                    /* note however that even when we find a winner,
                       we continue looping over @seqs to do housekeeping */
                }
                if(!sv_cmp(seqhead, winner)) {
                    /* Once we have a winner (including the iteration
                       where we first found him), inc the head ptr
                       for any seq which had the winner as a head,
                       NULL out any seq which is now empty,
                       and adjust tails for consistency */

                    const int new_head = ++heads[s];
                    if(new_head > AvFILLp(seq)) {
                        SvREFCNT_dec(avptr[s]);
                        avptr[s] = NULL;
                    }
                    else {
                        HE* tail_entry;
                        SV* val;
                        /* Because we know this new seqhead used to be
                           a tail, we can assume it is in tails and has
                           a positive value, which we need to dec */
                        svp = av_fetch(seq, new_head, 0);
                        seqhead = *svp;
                        tail_entry = hv_fetch_ent(tails, seqhead, 0, 0);
                        val = HeVAL(tail_entry);
                        sv_dec(val);
                    }
                }
            }

            /* if we found no candidates, we are done building the MRO.
               !cand means no seqs have any entries left to check */
            if(!cand) {
                Safefree(heads);
                break;
            }

            /* If we had candidates, but nobody won, then the @ISA
               hierarchy is not C3-incompatible */
            if(!winner) {
                /* we have to do some cleanup before we croak */

                SvREFCNT_dec(retval);
                Safefree(heads);

                Perl_croak(aTHX_ "Inconsistent hierarchy during C3 merge of class '%s': "
                    "merging failed on parent '%s'", stashname, SvPV_nolen(cand));
            }
        }
    }
    else { /* @ISA was undefined or empty */
        /* build a retval containing only ourselves */
        retval = newAV();
        av_push(retval, newSVpvn(stashname, stashname_len));
    }

    /* we don't want anyone modifying the cache entry but us,
       and we do so by replacing it completely */
    SvREADONLY_on(retval);

    if(!made_mortal_cache) {
        SvREFCNT_inc(retval);
        hv_store(cache, stashname, stashname_len, (SV*)retval, 0);
    }

    return retval;
}

STATIC I32
__dopoptosub_at(const PERL_CONTEXT *cxstk, I32 startingblock) {
    I32 i;
    for (i = startingblock; i >= 0; i--) {
        if(CxTYPE((PERL_CONTEXT*)(&cxstk[i])) == CXt_SUB) return i;
    }
    return i;
}

XS(XS_Class_C3_XS_nextcan);
XS(XS_Class_C3_XS_nextcan)
{
    dVAR; dXSARGS;

    SV* self = ST(0);
    const I32 throw_nomethod = SvIVX(ST(1));
    register I32 cxix = cxstack_ix;
    register const PERL_CONTEXT *ccstack = cxstack;
    const PERL_SI *top_si = PL_curstackinfo;
    HV* selfstash;
    GV* cvgv;
    SV *stashname;
    const char *fq_subname;
    const char *subname;
    STRLEN fq_subname_len;
    STRLEN stashname_len;
    STRLEN subname_len;
    SV* sv;
    GV** gvp;
    AV* linear_av;
    SV** linear_svp;
    HV* cstash;
    GV* candidate = NULL;
    CV* cand_cv = NULL;
    const char *hvname;
    I32 entries;
    HV* nmcache;
    HE* cache_entry;
    SV* cachekey;
    int i;

    SP -= items;

    if(sv_isobject(self))
        selfstash = SvSTASH(SvRV(self));
    else
        selfstash = gv_stashsv(self, 0);

    assert(selfstash);

    hvname = HvNAME(selfstash);
    if (!hvname)
        Perl_croak(aTHX_ "Can't use anonymous symbol table for method lookup");

    /* This block finds the contextually-enclosing fully-qualified subname,
       much like looking at (caller($i))[3] until you find a real sub that
       isn't ANON, etc (also skips over pureperl next::method, etc) */
    for(i = 0; i < 2; i++) {
        cxix = __dopoptosub_at(ccstack, cxix);
        for (;;) {
            /* we may be in a higher stacklevel, so dig down deeper */
            while (cxix < 0) {
                if(top_si->si_type == PERLSI_MAIN)
                    Perl_croak(aTHX_ "next::method/next::can/maybe::next::method must be used in method context");
                top_si = top_si->si_prev;
                ccstack = top_si->si_cxstack;
                cxix = __dopoptosub_at(ccstack, top_si->si_cxix);
            }

            if(CxTYPE((PERL_CONTEXT*)(&ccstack[cxix])) != CXt_SUB
              || (PL_DBsub && GvCV(PL_DBsub) && ccstack[cxix].blk_sub.cv == GvCV(PL_DBsub))) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }

            {
                const I32 dbcxix = __dopoptosub_at(ccstack, cxix - 1);
                if (PL_DBsub && GvCV(PL_DBsub) && dbcxix >= 0 && ccstack[dbcxix].blk_sub.cv == GvCV(PL_DBsub)) {
                    if(CxTYPE((PERL_CONTEXT*)(&ccstack[dbcxix])) != CXt_SUB) {
                        cxix = dbcxix;
                        continue;
                    }
                }
            }

            cvgv = CvGV(ccstack[cxix].blk_sub.cv);

            if(!isGV(cvgv)) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }

            /* we found a real sub here */
            sv = sv_2mortal(newSV(0));

            gv_efullname3(sv, cvgv, NULL);

            fq_subname = SvPVX(sv);
            fq_subname_len = SvCUR(sv);

            subname = strrchr(fq_subname, ':');
            if(!subname)
                Perl_croak(aTHX_ "next::method/next::can/maybe::next::method cannot find enclosing method");

            subname++;
            subname_len = fq_subname_len - (subname - fq_subname);
            if(subname_len == 8 && strEQ(subname, "__ANON__")) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }
            break;
        }
        cxix--;
    }

    /* If we made it to here, we found our context */

    /* cachekey = "objpkg|context::method::name" */
    cachekey = sv_2mortal(newSVpv(hvname, 0));
    sv_catpvn(cachekey, "|", 1);
    sv_catsv(cachekey, sv);

    nmcache = get_hv("next::METHOD_CACHE", 1);
    if((cache_entry = hv_fetch_ent(nmcache, cachekey, 0, 0))) {
        SV* val = HeVAL(cache_entry);
        if(val == &PL_sv_undef) {
            if(throw_nomethod)
                Perl_croak(aTHX_ "No next::method '%s' found for %s", subname, hvname);
            XSRETURN_EMPTY;
        }
        XPUSHs(sv_2mortal(newRV_inc(val)));
        XSRETURN(1);
    }

    /* beyond here is just for cache misses, so perf isn't as critical */

    stashname_len = subname - fq_subname - 2;
    stashname = sv_2mortal(newSVpvn(fq_subname, stashname_len));

    linear_av = __mro_linear_isa_c3(aTHX_ selfstash, NULL, 0);

    linear_svp = AvARRAY(linear_av);
    entries = AvFILLp(linear_av) + 1;

    while (entries--) {
        SV* const linear_sv = *linear_svp++;
        assert(linear_sv);
        if(sv_eq(linear_sv, stashname))
            break;
    }

    if(entries > 0) {
        SV* sub_sv = sv_2mortal(newSVpv(subname, subname_len));
        HV* cc3_mro = get_hv("Class::C3::MRO", 0);

        while (entries--) {
            SV* const linear_sv = *linear_svp++;
            assert(linear_sv);

            if(cc3_mro) {
                HE* he_cc3_mro_class = hv_fetch_ent(cc3_mro, linear_sv, 0, 0);
                if(he_cc3_mro_class) {
                    SV* cc3_mro_class_sv = HeVAL(he_cc3_mro_class);
                    if(SvROK(cc3_mro_class_sv)) {
                        HV* cc3_mro_class = (HV*)SvRV(cc3_mro_class_sv);
                        SV** svp_cc3_mro_class_methods = hv_fetch(cc3_mro_class, "methods", 7, 0);
                        if(svp_cc3_mro_class_methods) {
                            SV* cc3_mro_class_methods_sv = *svp_cc3_mro_class_methods;
                            if(SvROK(cc3_mro_class_methods_sv)) {
                                HV* cc3_mro_class_methods = (HV*)SvRV(cc3_mro_class_methods_sv);
                                if(hv_exists_ent(cc3_mro_class_methods, sub_sv, 0))
                                    continue;
                            }
                        }
                    }
                }
            }

            cstash = gv_stashsv(linear_sv, FALSE);

            if (!cstash) {
                if (ckWARN(WARN_MISC))
                    Perl_warner(aTHX_ packWARN(WARN_MISC), "Can't locate package %"SVf" for @%s::ISA",
                        (void*)linear_sv, hvname);
                continue;
            }

            assert(cstash);

            gvp = (GV**)hv_fetch(cstash, subname, subname_len, 0);
            if (!gvp) continue;

            candidate = *gvp;
            assert(candidate);

            if (SvTYPE(candidate) != SVt_PVGV)
                gv_init(candidate, cstash, subname, subname_len, TRUE);
            if (SvTYPE(candidate) == SVt_PVGV && (cand_cv = GvCV(candidate)) && !GvCVGEN(candidate)) {
                SvREFCNT_dec(linear_av);
                SvREFCNT_inc((SV*)cand_cv);
                hv_store_ent(nmcache, newSVsv(cachekey), (SV*)cand_cv, 0);
                XPUSHs(sv_2mortal(newRV_inc((SV*)cand_cv)));
                XSRETURN(1);
            }
        }
    }

    SvREFCNT_dec(linear_av);
    hv_store_ent(nmcache, newSVsv(cachekey), &PL_sv_undef, 0);
    if(throw_nomethod)
        Perl_croak(aTHX_ "No next::method '%s' found for %s", subname, hvname);
    XSRETURN_EMPTY;
}

XS(XS_Class_C3_XS_calculateMRO);
XS(XS_Class_C3_XS_calculateMRO)
{
    dVAR; dXSARGS;

    SV* classname;
    HV* class_stash;
    HV* cache = NULL;
    AV* res;
    I32 res_items;
    I32 ret_items;
    SV** res_ptr;

    if(items < 1 || items > 2)
        croak("Usage: calculateMRO(classname[, cache])");

    classname = ST(0);
    if(items == 2) cache = (HV*)SvRV(ST(1));

    class_stash = gv_stashsv(classname, 0);
    if(!class_stash)
        Perl_croak(aTHX_ "No such class: '%s'!", SvPV_nolen(classname));

    res = __mro_linear_isa_c3(aTHX_ class_stash, cache, 0);

    res_items = ret_items = AvFILLp(res) + 1;
    res_ptr = AvARRAY(res);

    SP -= items;

    while(res_items--) {
        SV* res_item = *res_ptr++;
        XPUSHs(sv_2mortal(newSVsv(res_item)));
    }
    SvREFCNT_dec(res);

    PUTBACK;

    return;
}

XS(XS_Class_C3_XS_plsubgen);
XS(XS_Class_C3_XS_plsubgen)
{
    dVAR; dXSARGS;

    SP -= items;
    XPUSHs(sv_2mortal(newSViv(PL_sub_generation)));
    PUTBACK;
    return;
}

XS(XS_Class_C3_XS_calc_mdt);
XS(XS_Class_C3_XS_calc_mdt)
{
    dVAR; dXSARGS;

    SV* classname;
    HV* cache;
    HV* class_stash;
    AV* class_mro;
    HV* our_c3mro; /* $Class::C3::MRO{classname} */
    SV* has_ovf = NULL;
    HV* methods;
    I32 mroitems;

    /* temps */
    HV* hv;
    HE* he;
    SV** svp;

    if(items < 1 || items > 2)
        croak("Usage: calculate_method_dispatch_table(classname[, cache])");

    classname = ST(0);
    class_stash = gv_stashsv(classname, 0);
    if(!class_stash)
        Perl_croak(aTHX_ "No such class: '%s'!", SvPV_nolen(classname));

    if(items == 2) cache = (HV*)SvRV(ST(1));

    class_mro = __mro_linear_isa_c3(aTHX_ class_stash, cache, 0);

    our_c3mro = newHV();
    hv_store(our_c3mro, "MRO", 3, (SV*)newRV_noinc((SV*)class_mro), 0);

    hv = get_hv("Class::C3::MRO", 1);
    hv_store_ent(hv, classname, (SV*)newRV_noinc((SV*)our_c3mro), 0);

    methods = newHV();

    /* skip first entry */
    mroitems = AvFILLp(class_mro);
    svp = AvARRAY(class_mro) + 1;
    while(mroitems--) {
        SV* mro_class = *svp++;
        HV* mro_stash = gv_stashsv(mro_class, 0);

        if(!mro_stash) continue;

        if(!has_ovf) {
            SV** ovfp = hv_fetch(mro_stash, "()", 2, 0);
            if(ovfp) has_ovf = *ovfp;
        }

        hv_iterinit(mro_stash);
        while(he = hv_iternext(mro_stash)) {
            CV* code;
            SV* mskey;
            SV* msval;
            HE* ourent;
            HV* meth_hash;
            SV* orig;

            mskey = hv_iterkeysv(he);
            if(hv_exists_ent(methods, mskey, 0)) continue;

            msval = hv_iterval(mro_stash, he);
            if(SvTYPE(msval) != SVt_PVGV || !(code = GvCVu(msval)))
                continue;

            if((ourent = hv_fetch_ent(class_stash, mskey, 0, 0))) {
                SV* val = HeVAL(ourent);
                if(val && SvTYPE(val) == SVt_PVGV && GvCVu(val))
                    continue;
            }

            meth_hash = newHV();
            orig = newSVsv(mro_class);
            sv_catpvn(orig, "::", 2);
            sv_catsv(orig, mskey);
            hv_store(meth_hash, "orig", 4, orig, 0);
            hv_store(meth_hash, "code", 4, newRV_inc((SV*)code), 0);
            hv_store_ent(methods, mskey, newRV_noinc((SV*)meth_hash), 0);
        }
    }

    hv_store(our_c3mro, "methods", 7, newRV_noinc((SV*)methods), 0);
    if(has_ovf) hv_store(our_c3mro, "has_overload_fallback", 21, SvREFCNT_inc(has_ovf), 0);
    XSRETURN_EMPTY;
}

MODULE = Class::C3::XS	PACKAGE = Class::C3::XS

BOOT:
    newXS("Class::C3::XS::calculateMRO", XS_Class_C3_XS_calculateMRO, __FILE__);
    newXS("Class::C3::XS::_plsubgen", XS_Class_C3_XS_plsubgen, __FILE__);
    newXS("Class::C3::XS::_calculate_method_dispatch_table", XS_Class_C3_XS_calc_mdt, __FILE__);
    newXS("Class::C3::XS::_nextcan", XS_Class_C3_XS_nextcan, __FILE__);

