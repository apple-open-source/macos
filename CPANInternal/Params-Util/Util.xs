#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* Changes in 5.7 series mean that now IOK is only set if scalar is
   precisely integer but in 5.6 and earlier we need to do a more
   complex test  */
#if PERL_VERSION <= 6
#define DD_is_integer(sv) (SvIOK(sv) && (SvIsUV(val) ? SvUV(sv) == SvNV(sv) : SvIV(sv) == SvNV(sv)))
#else
#define DD_is_integer(sv) SvIOK(sv)
#endif

static int
is_string0( SV *sv )
{
    return SvFLAGS(sv) & (SVf_OK & ~SVf_ROK);
}

static int
is_string( SV *sv )
{
    STRLEN len = 0;
    if( is_string0(sv) )
    {
        const char *pv = SvPV(sv, len);
    }
    return len;
}

static int
is_array( SV *sv )
{
    return SvROK(sv) && ( SVt_PVAV == SvTYPE(SvRV(sv) ) );
}

static int
is_hash( SV *sv )
{
    return SvROK(sv) && ( SVt_PVHV == SvTYPE(SvRV(sv) ) );
}

static int
is_like( SV *sv, const char *like )
{
    int likely = 0;
    if( sv_isobject( sv ) )
    {
        dSP;
        int count;

        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs( sv_2mortal( newSVsv( sv ) ) );
        XPUSHs( sv_2mortal( newSVpv( like, strlen(like) ) ) );
        PUTBACK;

        if( ( count = call_pv("overload::Method", G_SCALAR) ) )
        {
            I32 ax;
            SPAGAIN;

            SP -= count;
            ax = (SP - PL_stack_base) + 1;
            if( SvTRUE(ST(0)) )
                ++likely;
        }

        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    return likely;
}

MODULE = Params::Util		PACKAGE = Params::Util

void
_STRING(sv)
    SV *sv
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(sv) )
        mg_get(sv);
    if( is_string( sv ) )
    {
        ST(0) = sv;
        XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

void
_NUMBER(sv)
    SV *sv;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(sv) )
        mg_get(sv);
    if( ( SvIOK(sv) ) || ( SvNOK(sv) ) || ( is_string( sv ) && looks_like_number( sv ) ) )
    {
        ST(0) = sv;
        XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

void
_SCALAR0(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        if( ( SvTYPE(SvRV(ref)) <= SVt_PVBM ) && !sv_isobject(ref) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

void
_SCALAR(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        svtype tp = SvTYPE(SvRV(ref));
        if( ( SvTYPE(SvRV(ref)) <= SVt_PVBM ) && (!sv_isobject(ref)) && is_string( SvRV(ref) ) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

void
_REGEX(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        svtype tp = SvTYPE(SvRV(ref));
#if PERL_VERSION >= 11
        if( ( SVt_REGEXP == tp ) )
#else
        if( ( SVt_PVMG == tp ) && sv_isobject(ref)
         && ( 0 == strncmp( "Regexp", sv_reftype(SvRV(ref),TRUE),
                            strlen("Regexp") ) ) )
#endif
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

void
_ARRAY0(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( is_array(ref) )
    {
        ST(0) = ref;
        XSRETURN(1);
    }

    XSRETURN_UNDEF;
}

void
_ARRAY(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( is_array(ref) && ( av_len((AV *)(SvRV(ref))) >= 0 ) )
    {
        ST(0) = ref;
        XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

void
_ARRAYLIKE(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        if( is_array(ref) || is_like( ref, "@{}" ) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }

    XSRETURN_UNDEF;
}

void
_HASH0(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( is_hash(ref) )
    {
        ST(0) = ref;
        XSRETURN(1);
    }

    XSRETURN_UNDEF;
}

void
_HASH(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( is_hash(ref) && ( HvKEYS(SvRV(ref)) >= 1 ) )
    {
        ST(0) = ref;
        XSRETURN(1);
    }

    XSRETURN_UNDEF;
}

void
_HASHLIKE(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        if( is_hash(ref) || is_like( ref, "%{}" ) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }

    XSRETURN_UNDEF;
}

void
_CODE(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        if( SVt_PVCV == SvTYPE(SvRV(ref)) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

void
_CODELIKE(ref)
    SV *ref;
PROTOTYPE: $
CODE:
{
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) )
    {
        if( ( SVt_PVCV == SvTYPE(SvRV(ref)) ) || ( is_like(ref, "&{}" ) ) )
        {
            ST(0) = ref;
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

void
_INSTANCE(ref,type)
    SV *ref;
    char *type;
PROTOTYPE: $$
CODE:
{
    STRLEN len;
    if( SvMAGICAL(ref) )
        mg_get(ref);
    if( SvROK(ref) && type && ( ( len = strlen(type) ) > 0 ) )
    {
        if( sv_isobject(ref) )
        {
            I32 isa_type = 0;
            int count;

            ENTER;
            SAVETMPS;
            PUSHMARK(SP);
            XPUSHs( sv_2mortal( newSVsv( ref ) ) );
            XPUSHs( sv_2mortal( newSVpv( type, len ) ) );
            PUTBACK;

            if( ( count = call_method("isa", G_SCALAR) ) )
            {
                I32 oldax = ax;
                SPAGAIN;
                SP -= count;
                ax = (SP - PL_stack_base) + 1;
                isa_type = SvTRUE(ST(0));
                ax = oldax;
            }

            PUTBACK;
            FREETMPS;
            LEAVE;

            if( isa_type )
            {
                ST(0) = ref;
                XSRETURN(1);
            }
        }
    }
    XSRETURN_UNDEF;
}

