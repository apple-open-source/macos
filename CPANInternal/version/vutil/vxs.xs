#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "vutil.h"

/* --------------------------------------------------
 * $Revision: 2.5 $
 * --------------------------------------------------*/

typedef     SV *version_vxs;

MODULE = version::vxs	PACKAGE = version::vxs

PROTOTYPES: DISABLE
VERSIONCHECK: DISABLE

BOOT:
	/* register the overloading (type 'A') magic */
	PL_amagic_generation++;
	newXS("version::vxs::()", XS_version__vxs_noop, file);
	newXS("version::vxs::(\"\"", XS_version__vxs_stringify, file);
	newXS("version::vxs::(0+", XS_version__vxs_numify, file);
	newXS("version::vxs::(cmp", XS_version__vxs_vcmp, file);
	newXS("version::vxs::(<=>", XS_version__vxs_vcmp, file);
	newXS("version::vxs::(bool", XS_version__vxs_boolean, file);
	newXS("version::vxs::(nomethod", XS_version__vxs_noop, file);
	newXS("UNIVERSAL::VERSION", XS_version__vxs_VERSION, file);

void
new(...)
PPCODE:
{
    SV *vs = ST(1);
    SV *rv;
    char *class;

    /* get the class if called as an object method */
    if ( sv_isobject(ST(0)) ) {
	class = HvNAME(SvSTASH(SvRV(ST(0))));
    }
    else {
	class = (char *)SvPV_nolen(ST(0));
    }

    if (items == 3 )
    {
	STRLEN n_a;
	vs = sv_newmortal();
	sv_setpvf(vs,"v%s",SvPV(ST(2),n_a));
    }
    if ( items == 1 )
    {
	/* no parameter provided */
	if ( sv_isobject(ST(0)) )
	{
	    /* create empty object */
	    vs = sv_newmortal();
	    sv_setpv(vs,"");
	}
    }

    rv = new_version(vs);
    if ( strcmp(class,"version::vxs") != 0 ) /* inherited new() */
	sv_bless(rv, gv_stashpv(class,TRUE));

    PUSHs(sv_2mortal(rv));
}

void
stringify (lobj,...)
    version_vxs	lobj
PPCODE:
{
    PUSHs(sv_2mortal(vstringify(lobj)));
}

void
numify (lobj,...)
    version_vxs	lobj
PPCODE:
{
    PUSHs(sv_2mortal(vnumify(lobj)));
}

void
vcmp (lobj,...)
    version_vxs	lobj
PPCODE:
{
    SV	*rs;
    SV * robj = ST(1);
    IV	 swap = (IV)SvIV(ST(2));

    if ( ! sv_derived_from(robj, "version::vxs") )
    {
	robj = sv_2mortal(new_version(robj));
    }

    if ( swap )
    {
        rs = newSViv(vcmp(robj,lobj));
    }
    else
    {
        rs = newSViv(vcmp(lobj,robj));
    }

    PUSHs(sv_2mortal(rs));
}

void
boolean(lobj,...)
    version_vxs	lobj
PPCODE:
{
    SV	*rs;
    rs = newSViv( vcmp(lobj,new_version(newSVpvn("0",1))) );
    PUSHs(sv_2mortal(rs));
}

void
noop(lobj,...)
    version_vxs	lobj
CODE:
{
    Perl_croak(aTHX_ "operation not supported with version object");
}

void
is_alpha(lobj)
    version_vxs	lobj	
PPCODE:
{
    if ( hv_exists((HV*)SvRV(lobj), "alpha", 5 ) )
	XSRETURN_YES;
    else
	XSRETURN_NO;
}

void
qv(ver)
    SV *ver
PPCODE:
{
#ifdef SvVOK
    if ( !SvVOK(ver) ) { /* not already a v-string */
#endif
	SV *vs = sv_newmortal();
	char *version;
	if ( SvNOK(ver) ) /* may get too much accuracy */
	{
	    char tbuf[64];
	    sprintf(tbuf,"%.9"NVgf, SvNVX(ver));
	    version = savepv(tbuf);
	}
	else
	{
	    STRLEN n_a;
	    version = savepv(SvPV(ver,n_a));
	}
	(void)scan_version(version,vs,TRUE);
	Safefree(version);

	PUSHs(vs);
#ifdef SvVOK
    }
    else
    {
	PUSHs(sv_2mortal(new_version(ver)));
    }
#endif
}

void
normal(ver)
    SV *ver
PPCODE:
{
    PUSHs(sv_2mortal(vnormal(ver)));
}

void
VERSION(sv,...)
    SV *sv
PPCODE:
{
    HV *pkg;
    GV **gvp;
    GV *gv;
    char *undef;

    if (SvROK(sv)) {
        sv = (SV*)SvRV(sv);
        if (!SvOBJECT(sv))
            Perl_croak(aTHX_ "Cannot find version of an unblessed reference");
        pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(sv, FALSE);
    }

    gvp = pkg ? (GV**)hv_fetch(pkg,"VERSION",7,FALSE) : Null(GV**);

    if (gvp && isGV(gv = *gvp) && SvOK(sv = GvSV(gv))) {
        SV *nsv = sv_newmortal();
        sv_setsv(nsv, sv);
        sv = nsv;
	if ( !sv_derived_from(sv, "version::vxs"))
	    upg_version(sv);
        undef = Nullch;
    }
    else {
        sv = (SV*)&PL_sv_undef;
        undef = "(undef)";
    }

    if (items > 1) {
	SV *req = ST(1);
	STRLEN len;

	if (undef) {
	     if (pkg)
		  Perl_croak(aTHX_ "%s does not define $%s::VERSION--version check failed",
			     HvNAME(pkg), HvNAME(pkg));
	     else {
		  char *str = SvPVx(ST(0), len);

		  Perl_croak(aTHX_ "%s defines neither package nor VERSION--version check failed", str);
	     }
	}

        if ( !sv_derived_from(req, "version::vxs")) {
	    /* req may very well be R/O, so create a new object */
	    SV *nsv = sv_newmortal();
	    sv_setsv(nsv, req);
	    req = nsv;
	    upg_version(req);
	}

	if ( vcmp( req, sv ) > 0 )
	    Perl_croak(aTHX_ "%s version %"SVf" (%"SVf") required--"
		    "this is only version %"SVf" (%"SVf")", HvNAME(pkg),
		    vnumify(req),vnormal(req),vnumify(sv),vnormal(sv));
    }

    if ( SvOK(sv) && sv_derived_from(sv, "version::vxs") )
	PUSHs(vnumify(sv));
    else
	PUSHs(sv);

    XSRETURN(1);
}
