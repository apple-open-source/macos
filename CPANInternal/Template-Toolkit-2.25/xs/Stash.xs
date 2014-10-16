/*=====================================================================
*
* Template::Stash::XS (Stash.xs)
*
* DESCRIPTION
*   This is an XS implementation of the Template::Stash module.
*   It is an alternative version of the core Template::Stash methods
*   ''get'' and ''set'' (the ones that should benefit most from a
*   speedy C implementation), along with some virtual methods (like
*   first, last, reverse, etc.)
*
* AUTHORS
*   Andy Wardley   <abw@cpan.org>
*   Doug Steinwand <dsteinwand@citysearch.com>
*
* COPYRIGHT
*   Copyright (C) 1996-2012 Andy Wardley.  All Rights Reserved.
*   Copyright (C) 1998-2000 Canon Research Centre Europe Ltd.
*
*   This module is free software; you can redistribute it and/or
*   modify it under the same terms as Perl itself.
*
* NOTE
*   Be very familiar with the perlguts, perlxs, perlxstut and 
*   perlapi manpages before digging through this code.
*
*=====================================================================*/


#ifdef __cplusplus
extern "C" {
#endif

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#define NEED_sv_2pv_flags
#define NEED_newRV_noinc
#include "ppport.h"
#include "XSUB.h"

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER) || defined(__SUNPRO_C)
#define debug()
#else
#ifdef WIN32
#define debug(format)
#else
#define debug(...)
/* #define debug(...) fprintf(stderr, __VA_ARGS__) */
#endif
#endif

#ifdef WIN32
#define snprintf _snprintf
#endif

#define TT_STASH_PKG    "Template::Stash::XS"
#define TT_LIST_OPS     "Template::Stash::LIST_OPS"
#define TT_HASH_OPS     "Template::Stash::HASH_OPS"
#define TT_SCALAR_OPS   "Template::Stash::SCALAR_OPS"
#define TT_PRIVATE      "Template::Stash::PRIVATE"

#define TT_LVALUE_FLAG  1
#define TT_DEBUG_FLAG   2
#define TT_DEFAULT_FLAG 4

typedef enum tt_ret { TT_RET_UNDEF, TT_RET_OK, TT_RET_CODEREF } TT_RET;

static TT_RET   hash_op(pTHX_ SV*, char*, AV*, SV**, int);
static TT_RET   list_op(pTHX_ SV*, char*, AV*, SV**);
static TT_RET   scalar_op(pTHX_ SV*, char*, AV*, SV**, int);
static TT_RET   tt_fetch_item(pTHX_ SV*, SV*, AV*, SV**);
static TT_RET   autobox_list_op(pTHX_ SV*, char*, AV*, SV**, int);
static SV*      dotop(pTHX_ SV*, SV*, AV*, int);
static SV*      call_coderef(pTHX_ SV*, AV*);
static SV*      fold_results(pTHX_ I32);
static SV*      find_perl_op(pTHX_ char*, char*);
static AV*      mk_mortal_av(pTHX_ SV*, AV*, SV*);
static SV*      do_getset(pTHX_ SV*, AV*, SV*, int);
static AV*      convert_dotted_string(pTHX_ const char*, I32);
static int      get_debug_flag(pTHX_ SV*);
static int      cmp_arg(const void *, const void *);
static int      looks_private(pTHX_ const char*);
static void     die_object(pTHX_ SV *);
static struct xs_arg *find_xs_op(char *);
static SV*      list_dot_first(pTHX_ AV*, AV*);
static SV*      list_dot_join(pTHX_ AV*, AV*);
static SV*      list_dot_last(pTHX_ AV*, AV*);
static SV*      list_dot_max(pTHX_ AV*, AV*);
static SV*      list_dot_reverse(pTHX_ AV*, AV*);
static SV*      list_dot_size(pTHX_ AV*, AV*);
static SV*      hash_dot_each(pTHX_ HV*, AV*);
static SV*      hash_dot_keys(pTHX_ HV*, AV*);
static SV*      hash_dot_values(pTHX_ HV*, AV*);
static SV*      scalar_dot_defined(pTHX_ SV*, AV*);
static SV*      scalar_dot_length(pTHX_ SV*, AV*);

#define THROW_SIZE 64
static char throw_fmt[] = "Can't locate object method \"%s\" via package \"%s\"";

/* dispatch table for XS versions of special "virtual methods",
 * names must be in alphabetical order          
 */
static const struct xs_arg {
        const char *name;
        SV* (*list_f)   (pTHX_ AV*, AV*);
        SV* (*hash_f)   (pTHX_ HV*, AV*);
        SV* (*scalar_f) (pTHX_ SV*, AV*);
} xs_args[] = {
    /* name      list (AV) ops.    hash (HV) ops.   scalar (SV) ops.
       --------  ----------------  ---------------  ------------------  */
    { "defined", NULL,             NULL,            scalar_dot_defined  },
    { "each",    NULL,             hash_dot_each,   NULL                },
/*  { "first",   list_dot_first,   NULL,            NULL                }, */
    { "join",    list_dot_join,    NULL,            NULL                },
    { "keys",    NULL,             hash_dot_keys,   NULL                },
/*  { "last",    list_dot_last,    NULL,            NULL                }, */
    { "length",  NULL,             NULL,            scalar_dot_length   },
    { "max",     list_dot_max,     NULL,            NULL                },
    { "reverse", list_dot_reverse, NULL,            NULL                },
    { "size",    list_dot_size,    NULL,            NULL                },
    { "values",  NULL,             hash_dot_values, NULL                },
};



/*------------------------------------------------------------------------
 * tt_fetch_item(pTHX_ SV *root, SV *key_sv, AV *args, SV **result)
 *
 * Retrieves an item from the given hash or array ref.  If item is found
 * and a coderef then the coderef will be called and passed args.  Returns
 * TT_RET_CODEREF or TT_RET_OK and sets result.  If not found, returns 
 * TT_RET_UNDEF and result is undefined.
 *------------------------------------------------------------------------*/

static TT_RET tt_fetch_item(pTHX_ SV *root, SV *key_sv, AV *args, SV **result) {
    STRLEN key_len;
    char *key = SvPV(key_sv, key_len);
    SV **value = NULL;

#ifndef WIN32
    debug("fetch item: %s\n", key);
#endif

    /* negative key_len is used to indicate UTF8 string */
    if (SvUTF8(key_sv))
        key_len = -key_len;
    
    if (!SvROK(root)) 
        return TT_RET_UNDEF;
    
    switch (SvTYPE(SvRV(root))) {
      case SVt_PVHV:
        value = hv_fetch((HV *) SvRV(root), key, key_len, FALSE);
        break;

      case SVt_PVAV:
        if (looks_like_number(key_sv))
            value = av_fetch((AV *) SvRV(root), SvIV(key_sv), FALSE);
        break;
    }

    if (value) {
        /* trigger any tied magic to FETCH value */
        SvGETMAGIC(*value);
        
        /* call if a coderef */
        if (SvROK(*value) 
            && (SvTYPE(SvRV(*value)) == SVt_PVCV) 
            && !sv_isobject(*value)) {
            *result = call_coderef(aTHX_ *value, args);
            return TT_RET_CODEREF;
            
        } 
        else if (SvOK(*value)) {
            *result = *value;
            return TT_RET_OK;
        }

    } 

    *result = &PL_sv_undef;
    return TT_RET_UNDEF;
}



/*------------------------------------------------------------------------
 * dotop(pTHX_ SV *root, SV *key_sv, AV *args, int flags)
 *
 * Resolves dot operations of the form root.key, where 'root' is a
 * reference to the root item, 'key_sv' is an SV containing the
 * operation key (e.g. hash key, list index, first, last, each, etc),
 * 'args' is a list of additional arguments and 'TT_LVALUE_FLAG' is a 
 * flag to indicate if, for certain operations (e.g. hash key), the item
 * should be created if it doesn't exist.  Also, 'TT_DEBUG_FLAG' is the 
 * debug flag.
 *------------------------------------------------------------------------*/

static SV *dotop(pTHX_ SV *root, SV *key_sv, AV *args, int flags) {
    dSP;
    STRLEN item_len;
    char *item = SvPV(key_sv, item_len);
    SV *result = &PL_sv_undef;
    I32 atroot;

#ifndef WIN32
    debug("dotop(%s)\n", item);
#endif

    /* ignore _private or .private members */
    if (!root || looks_private(aTHX_ item))
        return &PL_sv_undef;
    
    if (SvROK(root)) {
        atroot = sv_derived_from(root, TT_STASH_PKG);

        if (atroot || ((SvTYPE(SvRV(root)) == SVt_PVHV) && !sv_isobject(root))) {
            /* root is a HASH or Template::Stash */
            switch(tt_fetch_item(aTHX_ root, key_sv, args, &result)) {
            case TT_RET_OK:
                /* return immediately */
                return result;
                break;
                
            case TT_RET_CODEREF:
                /* fall through */
                break;
                
            default:
                /* for lvalue, create an intermediate hash */
                if (flags & TT_LVALUE_FLAG) {
                    SV *newhash;
                    HV *roothv = (HV *) SvRV(root);
                    newhash = SvREFCNT_inc((SV *) newRV_noinc((SV *) newHV()));

                    debug("- auto-vivifying intermediate hash\n");

                    if (hv_store(roothv, item, item_len, newhash, 0)) {
                        /* trigger any tied magic to STORE value */
                        SvSETMAGIC(newhash);
                    }
                    else {
                        SvREFCNT_dec(newhash);
                    }
                    return sv_2mortal(newhash);
                }

                /* try hash virtual method (not at stash root, except import) */
                if ((! atroot || (strcmp(item, "import") == 0))
                    && hash_op(aTHX_ root, item, args, &result, flags) == TT_RET_UNDEF) {
                    /* try hash slice */ 
                    if (SvROK(key_sv) && SvTYPE(SvRV(key_sv)) == SVt_PVAV) {
                        AV *a_av = newAV();
                        AV *k_av = (AV *) SvRV(key_sv);
                        HV *r_hv = (HV *) SvRV(root);
                        char *t;
                        I32 i;
                        STRLEN tlen;
                        SV **svp;
                        
                        for (i = 0; i <= av_len(k_av); i++) {
                            if ((svp = av_fetch(k_av, i, 0))) {
                                SvGETMAGIC(*svp);
                                t = SvPV(*svp, tlen);
                                if((svp = hv_fetch(r_hv, t, tlen, FALSE))) {
                                    SvGETMAGIC(*svp);
                                    av_push(a_av, SvREFCNT_inc(*svp));
                                }
                            }
                        }
                        
                        return sv_2mortal(newRV_noinc((SV *) a_av));
                    }
                }
            }
            
        }
        else if ((SvTYPE(SvRV(root)) == SVt_PVAV) && !sv_isobject(root)) {
            /* root is an ARRAY, try list virtuals */
            if (list_op(aTHX_ root, item, args, &result) == TT_RET_UNDEF) {
                switch (tt_fetch_item(aTHX_ root, key_sv, args, &result)) {
                  case TT_RET_OK:
                    return result;
                    break;
                    
                  case TT_RET_CODEREF:
                    break;
                    
                  default:
                    /* try array slice */ 
                    if (SvROK(key_sv) && SvTYPE(SvRV(key_sv)) == SVt_PVAV) {
                        AV *a_av = newAV();
                        AV *k_av = (AV *) SvRV(key_sv);
                        AV *r_av = (AV *) SvRV(root);
                        I32 i;
                        SV **svp;
                        
                        for (i = 0; i <= av_len(k_av); i++) {
                            if ((svp = av_fetch(k_av, i, FALSE))) {
                                SvGETMAGIC(*svp);
                                if (looks_like_number(*svp) && 
                                    (svp = av_fetch(r_av, SvIV(*svp), FALSE))) {
                                    SvGETMAGIC(*svp);
                                    av_push(a_av, SvREFCNT_inc(*svp));
                                }
                            }
                        }
                        
                        return sv_2mortal(newRV_noinc((SV *) a_av));
                    }
                }
            }
        }
        else if (sv_isobject(root)) {
            /* root is an object */
            I32 n, i;
            SV **svp;
            HV *stash = SvSTASH((SV *) SvRV(root));
            GV *gv;
            /* char *error_string; */
            result = NULL;
            
            if ((gv = gv_fetchmethod_autoload(stash, item, 1))) {
                /* eval { @result = $root->$item(@$args); }; */
                
                PUSHMARK(SP);
                XPUSHs(root);
                n = (args && args != Nullav) ? av_len(args) : -1;
                for (i = 0; i <= n; i++)
                    if ((svp = av_fetch(args, i, 0))) XPUSHs(*svp);
                PUTBACK;
                n = call_method(item, G_ARRAY | G_EVAL);
                SPAGAIN;
                
                if (SvTRUE(ERRSV)) {
                    char throw_str[THROW_SIZE+1];
                    (void) POPs;                /* remove undef from stack */
                    PUTBACK;
                    result = NULL;
                    
                    /* if we get an exception object throw ($@ is a
                     * ref) or a error other than "Can't locate object
                     * method "blah"" then it's a real error that need
                     * to be re-thrown.
                     */
                    
                    if (SvROK(ERRSV)) {
                        die_object(aTHX_ ERRSV);
                    }
                    else {

                        /* We use throw_str to construct the error message
                         * that indicates a missing method. We use snprintf() to
                         * avoid overflowing throw_str, and always ensure the
                         * last character is NULL (if the item name is too long
                         * to fit into throw_str then snprintf() doesn't add the
                         * terminating NULL 
                         */
                        snprintf(throw_str, THROW_SIZE, throw_fmt, item, HvNAME(stash));
                        throw_str[THROW_SIZE] = '\0';

                        if (! strstr( SvPV(ERRSV, PL_na), throw_str)) 
                            die_object(aTHX_ ERRSV);
                    }
                } else {
                    result = fold_results(aTHX_ n);
                }
            }
            
            if (!result) {
                /* failed to call object method, so try some fallbacks */
                if (SvTYPE(SvRV(root)) == SVt_PVHV) {
                    /* hash based object - first try to fetch item */
                    switch(tt_fetch_item(aTHX_ root, key_sv, args, &result)) {
                    case TT_RET_OK:
                        /* return immediately */
                        return result;
                        break;
                
                    case TT_RET_CODEREF:
                        /* fall through */
                        break;
                
                    default:
                        /* then try hash vmethod if that failed */
                        if (hash_op(aTHX_ root, item, args, &result, flags) == TT_RET_OK) 
                            return result;
                        /* hash_op() will also try list_op([$hash]) */
                    }
                }
                else if (SvTYPE(SvRV(root)) == SVt_PVAV) {
                    /* list based object - first try to fetch item */
                    switch (tt_fetch_item(aTHX_ root, key_sv, args, &result)) {
                    case TT_RET_OK:
                        /* return immediately */
                        return result;
                        break;
                        
                    case TT_RET_CODEREF:
                        /* fall through */
                        break;
                
                    default:
                        /* try list vmethod */
                        if (list_op(aTHX_ root, item, args, &result) == TT_RET_OK) 
                            return result;
                    }
                }
                else if (scalar_op(aTHX_ root, item, args, &result, flags) == TT_RET_OK) {
                    /* scalar_op() will also try list_op([$scalar]) */
                    return result;
                }
                else if (flags & TT_DEBUG_FLAG) {
                    result = (SV *) mk_mortal_av(aTHX_ &PL_sv_undef, NULL, ERRSV);
                }
            }
        }
    }
    /* it doesn't look like we've got a reference to anything we know about,
     * so let's try the SCALAR_OPS pseudo-methods (but not for l-values) 
     */
    
    else if (!(flags & TT_LVALUE_FLAG) 
             && (scalar_op(aTHX_ root, item, args, &result, flags)
                 == TT_RET_UNDEF)) {
        if (flags & TT_DEBUG_FLAG)
            croak("don't know how to access [ %s ].%s\n", 
                  SvPV(root, PL_na), item);
    }
    
    /* if we have an arrayref and the first element is defined then 
     * everything is peachy, otherwise some ugliness may have occurred 
     */
    
    if (SvROK(result) && SvTYPE(SvRV(result)) == SVt_PVAV) {
        SV **svp;
        AV *array = (AV *) SvRV(result);
        I32 len = (array == Nullav) ? 0 : (av_len(array) + 1);
        
        if (len) {
            svp = av_fetch(array, 0, FALSE);
            if (svp && (*svp != &PL_sv_undef)) {
                return result;
            }
        }
    } 
    
    if ((flags & TT_DEBUG_FLAG) 
        && (!result || !SvOK(result) || (result == &PL_sv_undef))) {
        croak("%s is undefined\n", item);
    }
    
    return result;
}



/*------------------------------------------------------------------------
 * assign(pTHX_ SV *root, SV *key_sv, AV *args, SV *value, int flags)
 *
 * Resolves the final assignment element of a dotted compound variable
 * of the form "root.key(args) = value".  'root' is a reference to
 * the root item, 'key_sv' is an SV containing the operation key
 * (e.g. hash key, list item, object method), 'args' is a list of user
 * provided arguments (passed only to object methods), 'value' is the
 * assignment value to be set (appended to args) and 'deflt' (default)
 * is a flag to indicate that the assignment should only be performed
 * if the item is currently undefined/false.
 *------------------------------------------------------------------------*/

static SV *assign(pTHX_ SV *root, SV *key_sv, AV *args, SV *value, int flags) {
    dSP;
    SV **svp, *newsv;
    HV *roothv;
    AV *rootav;
    STRLEN key_len;
    char *key = SvPV(key_sv, key_len);
    char *key2 = SvPV(key_sv, key_len);     /* TMP DEBUG HACK */

#ifndef WIN32
    debug("assign(%s)\n", key2);
#endif

    /* negative key_len is used to indicate UTF8 string */
    if (SvUTF8(key_sv))
        key_len = -key_len;

    if (!root || !SvOK(key_sv) || key_sv == &PL_sv_undef || looks_private(aTHX_ key)) {
        /* ignore _private or .private members */
        return &PL_sv_undef;
    } 
    else if (SvROK(root)) {
        /* see if root is an object (but not Template::Stash) */
        if (sv_isobject(root) && !sv_derived_from(root, TT_STASH_PKG)) {
            HV *stash = SvSTASH((SV *) SvRV(root));
            GV *gv;

            /* look for the named method, or an AUTOLOAD method */
            if ((gv = gv_fetchmethod_autoload(stash, key, 1))) {
                I32 count = (args && args != Nullav) ? av_len(args) : -1;
                I32 i;
                
                /* push args and value onto stack, then call method */
                PUSHMARK(SP);
                XPUSHs(root);
                for (i = 0; i <= count; i++) {
                    if ((svp = av_fetch(args, i, FALSE)))
                        XPUSHs(*svp);
                }
                XPUSHs(value);
                PUTBACK;
                debug(" - calling object method\n");
                count = call_method(key, G_ARRAY);
                SPAGAIN;
                return fold_results(aTHX_ count);               
            }
        }

        /* drop-through if not an object or method not found  */
        switch (SvTYPE(SvRV(root))) {        
            
        case SVt_PVHV:                              /* HASH */
            roothv = (HV *) SvRV(root);

            debug(" - hash assign\n");

            /* check for any existing value if ''default'' flag set */
            if ((flags & TT_DEFAULT_FLAG)
                && (svp = hv_fetch(roothv, key, key_len, FALSE))) {
                /* invoke any tied magical FETCH method */
                debug(" - fetched default\n");
                SvGETMAGIC(*svp);
                if (SvTRUE(*svp))
                    return &PL_sv_undef;
            }
            
            /* avoid 'modification of read-only value' error */
            newsv = newSVsv(value); 
            hv_store(roothv, key, key_len, newsv, 0);
            SvSETMAGIC(newsv);

            return value;
            break;

        case SVt_PVAV:                              /* ARRAY */
            rootav = (AV *) SvRV(root);

            debug(" - list assign\n");

            if (looks_like_number(key_sv)) {
                /* if the TT_DEFAULT_FLAG is set then first look to see if the 
                 * target is already set to some true value;  if it is then 
                 * we return that value (after invoking any SvGETMAGIC required
                 * for tied arrays) and bypass the assignment altogether
                 */

                if ( (flags & TT_DEFAULT_FLAG) 
                  && (svp = av_fetch(rootav, SvIV(key_sv), FALSE))) {

                    debug(" - fetched default, invoking any tied magic\n");
                    SvGETMAGIC(*svp);

                    if (SvTRUE(*svp))
                        return &PL_sv_undef;
                }

                /* create a new SV for the value and call av_store(),
                 * incrementing the reference count on the way; we
                 * then invoke any set magic for tied arrays; if the
                 * return value from av_store is NULL (as appears to
                 * be the case with tied arrays - although the same
                 * isn't true of hv_store() for some reason???) then
                 * we decrement the reference counter because that's
                 * what perlguts tells us to do...
                 */
                newsv = newSVsv(value);
                svp = av_store(rootav, SvIV(key_sv), newsv);
                SvSETMAGIC(newsv);

                return value;
            }
            else
                return &PL_sv_undef;
            
            break;

        default:                                    /* BARF */
            /* TODO: fix [ %s ] */
            croak("don't know how to assign to [ %s ].%s", 
                  SvPV(SvRV(root), PL_na), key);
        }
    }
    else {                                          /* SCALAR */
        /* TODO: fix [ %s ] */
        croak("don't know how to assign to [ %s ].%s", 
              SvPV(SvRV(root), PL_na), key);
    }
    
    /* not reached */
    return &PL_sv_undef;                            /* just in case */
}



/* dies and passes back a blessed object,  
 * or just a string if it's not blessed 
 */
static void die_object (pTHX_ SV *err) {

    if (sv_isobject(err) || SvROK(err)) {
        /* throw object via ERRSV ($@) */
        SV *errsv = get_sv("@", TRUE);
        sv_setsv(errsv, err);
        (void) die(Nullch);
    }

    /* error string sent back via croak() */
    croak("%s", SvPV(err, PL_na));
}


/* pushes any arguments in 'args' onto the stack then calls the code ref
 * in 'code'.  Calls fold_results() to return a listref or die.
 */
static SV *call_coderef(pTHX_ SV *code, AV *args) {
    dSP;
    SV **svp;
    I32 count = (args && args != Nullav) ? av_len(args) : -1;
    I32 i;

    PUSHMARK(SP);
    for (i = 0; i <= count; i++)
        if ((svp = av_fetch(args, i, FALSE))) 
            XPUSHs(*svp);
    PUTBACK;
    count = call_sv(code, G_ARRAY|G_EVAL);
    SPAGAIN;

    if (SvTRUE(ERRSV)) {
        die_object(aTHX_ ERRSV);
    }

    return fold_results(aTHX_ count);
}


/* pops 'count' items off the stack, folding them into a list reference
 * if count > 1, or returning the sole item if count == 1.  
 * Returns undef if count == 0. 
 * Dies if first value of list is undef
 */
static SV* fold_results(pTHX_ I32 count) {
    dSP;
    SV *retval = &PL_sv_undef;

    if (count > 1) {
        /* convert multiple return items into a list reference */
        AV *av = newAV();
        SV *last_sv = &PL_sv_undef;
        SV *sv = &PL_sv_undef;
        I32 i;

        av_extend(av, count - 1);
        for(i = 1; i <= count; i++) {
            last_sv = sv;
            sv = POPs; 
            if (SvOK(sv) && !av_store(av, count - i, SvREFCNT_inc(sv))) 
                SvREFCNT_dec(sv);
        }
        PUTBACK;
        
        retval = sv_2mortal((SV *) newRV_noinc((SV *) av));

        if (!SvOK(sv) || sv == &PL_sv_undef) {
            /* if first element was undef, die */
            die_object(aTHX_ last_sv);
        } 
        return retval;
        
    } else { 
        if (count)
            retval = POPs; 
        PUTBACK;
        return retval;
    }
}


/* Iterates through array calling dotop() to resolve all items
 * Skips the last if ''value'' is non-NULL.
 * If ''value'' is non-NULL, calls assign() to do the assignment.
 *
 * SV *root; AV *ident_av; SV *value; int flags;
 *
*/
static SV* do_getset(pTHX_ SV *root, AV *ident_av, SV *value, int flags) {
    AV *key_args;
    SV *key;
    SV **svp;
    I32 end_loop, i, size = av_len(ident_av);

    if (value) {
        /* make some adjustments for assign mode */
        end_loop = size - 1;
        flags |= TT_LVALUE_FLAG;
    } else {
        end_loop = size;
    }

    for(i = 0; i < end_loop; i += 2) {
        if (!(svp = av_fetch(ident_av, i, FALSE)))
            croak(TT_STASH_PKG " %cet: bad element %i", value ? 's' : 'g', i);

        key = *svp;

        if (!(svp = av_fetch(ident_av, i + 1, FALSE)))
            croak(TT_STASH_PKG " %cet: bad arg. %i", value ? 's' : 'g', i + 1);

        if (SvROK(*svp) && SvTYPE(SvRV(*svp)) == SVt_PVAV)
            key_args = (AV *) SvRV(*svp);
        else
            key_args = Nullav;
                
        root = dotop(aTHX_ root, key, key_args, flags);
    
        if (!root || !SvOK(root))
            return root;
    }

    if (value && SvROK(root)) {

        /* call assign() to resolve the last item */
        if (!(svp = av_fetch(ident_av, size - 1, FALSE)))
            croak(TT_STASH_PKG ": set bad ident element at %i", i);

        key = *svp;

        if (!(svp = av_fetch(ident_av, size, FALSE)))
            croak(TT_STASH_PKG ": set bad ident argument at %i", i + 1);
        
        if (SvROK(*svp) && SvTYPE(SvRV(*svp)) == SVt_PVAV)
            key_args = (AV *) SvRV(*svp);
        else
            key_args = Nullav;

        return assign(aTHX_ root, key, key_args, value, flags);
    }

    return root;
}


/* return [ map { s/\(.*$//; ($_, 0) } split(/\./, $str) ];
 */
static AV *convert_dotted_string(pTHX_ const char *str, I32 len) {
    AV *av = newAV();
    char *buf, *b;
    int b_len = 0;

    New(0, buf, len + 1, char);
    if (!buf) 
        croak(TT_STASH_PKG ": New() failed for convert_dotted_string");

    for(b = buf; len >= 0; str++, len--) {
        if (*str == '(') {
            for(; (len > 0) && (*str != '.'); str++, len--) ;
        } 
        if ((len < 1) || (*str == '.')) {
            *b = '\0';
            av_push(av, newSVpv(buf, b_len));
            av_push(av, newSViv((IV) 0));
            b = buf;
            b_len = 0;
        } else {
            *b++ = *str;
            b_len++;
        }
    }

    Safefree(buf);
    return (AV *) sv_2mortal((SV *) av);
}


/* performs a generic hash operation identified by 'key' 
 * (e.g. keys, * values, each) on 'hash'.
 * returns TT_RET_CODEREF if successful, TT_RET_UNDEF otherwise.
 */
static TT_RET hash_op(pTHX_ SV *root, char *key, AV *args, SV **result, int flags) {
    struct xs_arg *a;
    SV *code;
    TT_RET retval;

    /* look for XS version first */
    if ((a = find_xs_op(key)) && a->hash_f) {
        *result = a->hash_f(aTHX_ (HV *) SvRV(root), args);
        return TT_RET_CODEREF;
    }

    /* look for perl version in Template::Stash module */
    if ((code = find_perl_op(aTHX_ key, TT_HASH_OPS))) {
        *result = call_coderef(aTHX_ code, mk_mortal_av(aTHX_ root, args, NULL)); 
        return TT_RET_CODEREF;
    }
    
    /* try upgrading item to a list and look for a list op */
    if (!(flags & TT_LVALUE_FLAG)) {
        /* hash.method  ==>  [hash].method */
        return autobox_list_op(aTHX_ root, key, args, result, flags);
    }
    
    /* not found */
    *result = &PL_sv_undef;
    return TT_RET_UNDEF;
}


/* performs a generic list operation identified by 'key' on 'list'.  
 * Additional arguments may be passed in 'args'. 
 * returns TT_RET_CODEREF if successful, TT_RET_UNDEF otherwise.
 */
static TT_RET list_op(pTHX_ SV *root, char *key, AV *args, SV **result) {
    struct xs_arg *a;
    SV *code;

    /* look for and execute XS version first */
    if ((a = find_xs_op(key)) && a->list_f) {
#ifndef WIN32
        debug("calling internal list vmethod: %s\n", key);
#endif
        *result = a->list_f(aTHX_ (AV *) SvRV(root), args);
        return TT_RET_CODEREF;
    }

    /* look for and execute perl version in Template::Stash module */
    if ((code = find_perl_op(aTHX_ key, TT_LIST_OPS))) {
#ifndef WIN32
        debug("calling perl list vmethod: %s\n", key);
#endif
        *result = call_coderef(aTHX_ code, mk_mortal_av(aTHX_ root, args, NULL));
        return TT_RET_CODEREF;
    }

#ifndef WIN32
    debug("list vmethod not found: %s\n", key);
#endif

    /* not found */
    *result = &PL_sv_undef;
    return TT_RET_UNDEF;
}


/* Performs a generic scalar operation identified by 'key' 
 * on 'sv'.  Additional arguments may be passed in 'args'. 
 * returns TT_RET_CODEREF if successful, TT_RET_UNDEF otherwise.
 */
static TT_RET scalar_op(pTHX_ SV *sv, char *key, AV *args, SV **result, int flags) {
    struct xs_arg *a;
    SV *code;
    TT_RET retval;

    /* look for a XS version first */
    if ((a = find_xs_op(key)) && a->scalar_f) {
        *result = a->scalar_f(aTHX_ sv, args);
        return TT_RET_CODEREF;
    }

    /* look for perl version in Template::Stash module */
    if ((code = find_perl_op(aTHX_ key, TT_SCALAR_OPS))) {
        *result = call_coderef(aTHX_ code, mk_mortal_av(aTHX_ sv, args, NULL));
        return TT_RET_CODEREF;
    }

    /* try upgrading item to a list and look for a list op */
    if (!(flags & TT_LVALUE_FLAG)) {
        /* scalar.method  ==>  [scalar].method */
        return autobox_list_op(aTHX_ sv, key, args, result, flags);
    }

    /* not found */
    *result = &PL_sv_undef;
    return TT_RET_UNDEF;
}

static TT_RET autobox_list_op(pTHX_ SV *sv, char *key, AV *args, SV **result, int flags) {
    AV *av    = newAV();
    SV *avref = (SV *) newRV_inc((SV *) av);
    TT_RET retval;
    av_push(av, SvREFCNT_inc(sv)); 
    retval = list_op(aTHX_ avref, key, args, result);
    SvREFCNT_dec(av);
    SvREFCNT_dec(avref);
    return retval;
}

/* xs_arg comparison function */
static int cmp_arg(const void *a, const void *b) {
    return (strcmp(((const struct xs_arg *)a)->name,
                   ((const struct xs_arg *)b)->name));
}


/* Searches the xs_arg table for key */
static struct xs_arg *find_xs_op(char *key) {
    struct xs_arg *ap, tmp;

    tmp.name = key;
    if ((ap = (struct xs_arg *) 
         bsearch(&tmp, 
                 xs_args,
                 sizeof(xs_args)/sizeof(struct xs_arg), 
                 sizeof(struct xs_arg),
                 cmp_arg)))
        return ap;
    
    return NULL;
}


/* Searches the perl Template::Stash.pm module for ''key'' in the
 * hashref named ''perl_var''. Returns SV if found, NULL otherwise.
 */
static SV *find_perl_op(pTHX_ char *key, char *perl_var) {
    SV *tt_ops;
    SV **svp;

    if ((tt_ops = get_sv(perl_var, FALSE)) 
        && SvROK(tt_ops) 
        && (svp = hv_fetch((HV *) SvRV(tt_ops), key, strlen(key), FALSE)) 
        && SvROK(*svp) 
        && SvTYPE(SvRV(*svp)) == SVt_PVCV)
        return *svp;
    
    return NULL;
}


/* Returns: @a = ($sv, @av, $more) */
static AV *mk_mortal_av(pTHX_ SV *sv, AV *av, SV *more) {
    SV **svp;
    AV *a;
    I32 i = 0, size;

    a = newAV();
    av_push(a, SvREFCNT_inc(sv));

    if (av && (size = av_len(av)) > -1) {
        av_extend(a, size + 1);
        for (i = 0; i <= size; i++)
            if ((svp = av_fetch(av, i, FALSE))) 
                if(!av_store(a, i + 1, SvREFCNT_inc(*svp)))
                    SvREFCNT_dec(*svp);
    }
    
    if (more && SvOK(more))
        if (!av_store(a, i + 1, SvREFCNT_inc(more)))
            SvREFCNT_dec(more);
    
    return (AV *) sv_2mortal((SV *) a);
}

/* Returns TT_DEBUG_FLAG if _DEBUG key is true in hashref ''sv''. */
static int get_debug_flag (pTHX_ SV *sv) {
    const char *key = "_DEBUG";
    const I32 len = 6;
    SV **debug;
    
    if (SvROK(sv) 
        && (SvTYPE(SvRV(sv)) == SVt_PVHV) 
        && (debug = hv_fetch((HV *) SvRV(sv), (char *) key, len, FALSE))
        && SvOK(*debug)
        && SvTRUE(*debug)) 
        return TT_DEBUG_FLAG;
    
    return 0;
}


static int looks_private(pTHX_ const char *name) {
    /* SV *priv; */

    /* For now we hard-code the regex to match _private or .hidden
     * variables, but we do check to see if $Template::Stash::PRIVATE
     * is defined, allowing a user to undef it to defeat the check.
     * The better solution would be to match the string using the regex
     * defined in the $PRIVATE package varible, but I've been searching 
     * for well over an hour now and I can't find any documentation or 
     * examples showing me how to match a string against a pre-compiled 
     * regex from XS.  The Perl internals docs really suck in places.
     */
    
    if (SvTRUE(get_sv(TT_PRIVATE, FALSE))) {
        return (*name == '_' || *name == '.');
    }  
    return 0;
}


/* XS versions of some common dot operations 
 * ----------------------------------------- */

/* list.first */
static SV *list_dot_first(pTHX_ AV *list, AV *args) {
    SV **svp;
    if ((svp = av_fetch(list, 0, FALSE))) {
        /* entry fetched from arry may be code ref */
        if (SvROK(*svp) && SvTYPE(SvRV(*svp)) == SVt_PVCV) {
            return call_coderef(aTHX_ *svp, args);
        } else {
            return *svp;
        }
    }
    return &PL_sv_undef;
}


/* list.join */
static SV *list_dot_join(pTHX_ AV *list, AV *args) {
    SV **svp;
    SV *item, *retval;
    I32 size, i;
    STRLEN jlen;
    char *joint;

    if (args && (svp = av_fetch(args, 0, FALSE)) != NULL) {
        joint = SvPV(*svp, jlen);
    } else {
        joint = " ";
        jlen = 1;
    }

    retval = newSVpvn("", 0);
    size = av_len(list);
    for (i = 0; i <= size; i++) {
        if ((svp = av_fetch(list, i, FALSE)) != NULL) {
            item = *svp;
            if (SvROK(item) && SvTYPE(SvRV(item)) == SVt_PVCV) {
                item = call_coderef(aTHX_ *svp, args);
                sv_catsv(retval, item);
            } else {
                sv_catsv(retval, item);
            }
            if (i != size)
                sv_catpvn(retval, joint, jlen);
        }
    }
    return sv_2mortal(retval);
}


/* list.last */
static SV *list_dot_last(pTHX_ AV *list, AV *args) {
    SV **svp;
    if ((av_len(list) > -1)
        && (svp = av_fetch(list, av_len(list), FALSE))) {
        /* entry fetched from arry may be code ref */
        if (SvROK(*svp) && SvTYPE(SvRV(*svp)) == SVt_PVCV) {
            return call_coderef(aTHX_ *svp, args);
        } else {
            return *svp;
        }
    }
    return &PL_sv_undef;
}
 

/* list.max */
static SV *list_dot_max(pTHX_ AV *list, AV *args) {
    return sv_2mortal(newSViv((IV) av_len(list)));
}


/* list.reverse */
static SV *list_dot_reverse(pTHX_ AV *list, AV *args) {
    SV **svp;
    AV *result = newAV();
    I32 size, i;
            
    if ((size = av_len(list)) >= 0) {
        av_extend(result, size + 1);
        for (i = 0; i <= size; i++) {
            if ((svp = av_fetch(list, i, FALSE)) != NULL)
                if (!av_store(result, size - i, SvREFCNT_inc(*svp)))
                    SvREFCNT_dec(*svp);
        }
    }
    return sv_2mortal((SV *) newRV_noinc((SV *) result));
}


/* list.size */
static SV *list_dot_size(pTHX_ AV *list, AV *args) {
    return sv_2mortal(newSViv((IV) av_len(list) + 1));
}


/* hash.each */
static SV *hash_dot_each(pTHX_ HV *hash, AV *args) {
    AV *result = newAV();
    HE *he;
    hv_iterinit(hash);
    while ((he = hv_iternext(hash))) {
        av_push(result, SvREFCNT_inc((SV *) hv_iterkeysv(he)));
        av_push(result, SvREFCNT_inc((SV *) hv_iterval(hash, he)));
    }
    return sv_2mortal((SV *) newRV_noinc((SV *) result));
}


/* hash.keys */
static SV *hash_dot_keys(pTHX_ HV *hash, AV *args) {
    AV *result = newAV();
    HE *he;

    hv_iterinit(hash);
    while ((he = hv_iternext(hash)))
        av_push(result, SvREFCNT_inc((SV *) hv_iterkeysv(he)));
    
    return sv_2mortal((SV *) newRV_noinc((SV *) result));
}


/* hash.values */
static SV *hash_dot_values(pTHX_ HV *hash, AV *args) {
    AV *result = newAV();
    HE *he;

    hv_iterinit(hash);
    while ((he = hv_iternext(hash)))
        av_push(result, SvREFCNT_inc((SV *) hv_iterval(hash, he)));
    
    return sv_2mortal((SV *) newRV_noinc((SV *) result));
}


/* scalar.defined */
static SV *scalar_dot_defined(pTHX_ SV *sv, AV *args) {
    return &PL_sv_yes;
}


/* scalar.length */
static SV *scalar_dot_length(pTHX_ SV *sv, AV *args) {
    return sv_2mortal(newSViv((IV) SvUTF8(sv) ? sv_len_utf8(sv): sv_len(sv)));
}


/*====================================================================
 * XS SECTION                                                     
 *====================================================================*/

MODULE = Template::Stash::XS            PACKAGE = Template::Stash::XS

PROTOTYPES: DISABLED


#-----------------------------------------------------------------------
# get(SV *root, SV *ident, SV *args)
#-----------------------------------------------------------------------
SV *
get(root, ident, ...)
    SV *root
    SV *ident
    CODE:
    AV *args;
    int flags = get_debug_flag(aTHX_ root);
    int n;
    STRLEN len;
    char *str;

    /* look for a list ref of arguments, passed as third argument */
    args = 
        (items > 2 && SvROK(ST(2)) && SvTYPE(SvRV(ST(2))) == SVt_PVAV) 
        ? (AV *) SvRV(ST(2)) : Nullav;
     
    if (SvROK(ident) && (SvTYPE(SvRV(ident)) == SVt_PVAV)) {
        RETVAL = do_getset(aTHX_ root, (AV *) SvRV(ident), NULL, flags);

    } 
    else if (SvROK(ident)) {
        croak(TT_STASH_PKG ": get (arg 2) must be a scalar or listref");
    } 
    else if ((str = SvPV(ident, len)) && memchr(str, '.', len)) {
        /* convert dotted string into an array */
        AV *av = convert_dotted_string(aTHX_ str, len);
        RETVAL = do_getset(aTHX_ root, av, NULL, flags);
        av_undef(av);
    } 
    else {
        /* otherwise ident is a scalar so we call dotop() just once */
        RETVAL = dotop(aTHX_ root, ident, args, flags);
    }

    if (!SvOK(RETVAL)) {
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(root);
        XPUSHs(ident);
        PUTBACK;
        n = call_method("undefined", G_SCALAR);
        SPAGAIN;
        if (n != 1)
            croak("undefined() did not return a single value\n");
        RETVAL = SvREFCNT_inc(POPs);
        PUTBACK;
        FREETMPS;
        LEAVE;
    }
    else
        RETVAL = SvREFCNT_inc(RETVAL);

    OUTPUT:
    RETVAL



#-----------------------------------------------------------------------
# set(SV *root, SV *ident, SV *value, SV *deflt)
#-----------------------------------------------------------------------
SV *
set(root, ident, value, ...)
    SV *root
    SV *ident
    SV *value
    CODE:
    int flags = get_debug_flag(aTHX_ root);
    STRLEN len;
    char *str;

    /* check default flag passed as fourth argument */
    flags |= ((items > 3) && SvTRUE(ST(3))) ? TT_DEFAULT_FLAG : 0;

    if (SvROK(ident) && (SvTYPE(SvRV(ident)) == SVt_PVAV)) {
        RETVAL = do_getset(aTHX_ root, (AV *) SvRV(ident), value, flags);

    } 
    else if (SvROK(ident)) {
        croak(TT_STASH_PKG ": set (arg 2) must be a scalar or listref");

    }
    else if ((str = SvPV(ident, len)) && memchr(str, '.', len)) {
        /* convert dotted string into a temporary array */
        AV *av = convert_dotted_string(aTHX_ str, len);
        RETVAL = do_getset(aTHX_ root, av, value, flags);
        av_undef(av);
    } 
    else {
        /* otherwise a simple scalar so call assign() just once */
        RETVAL = assign(aTHX_ root, ident, Nullav, value, flags);
    }

    if (!SvOK(RETVAL))
        RETVAL = newSVpvn("", 0);       /* new empty string */
    else
        RETVAL = SvREFCNT_inc(RETVAL);
        
    OUTPUT:
    RETVAL


