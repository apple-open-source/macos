#include "vutil.h"

/*
=for apidoc scan_version

Returns a pointer to the next character after the parsed
version string, as well as upgrading the passed in SV to
an RV.

Function must be called with an already existing SV like

    sv = newSV(0);
    s = scan_version(s,SV *sv, bool qv);

Performs some preprocessing to the string to ensure that
it has the correct characteristics of a version.  Flags the
object if it contains an underscore (which denotes this
is a alpha version).  The boolean qv denotes that the version
should be interpreted as if it had multiple decimals, even if
it doesn't.

=cut
*/

const char *
Perl_scan_version(pTHX_ const char *s, SV *rv, bool qv)
{
    const char *start;
    const char *pos;
    const char *last;
    int saw_period = 0;
    int alpha = 0;
    int width = 3;
    AV *av = newAV();
    SV *hv = newSVrv(rv, "version"); /* create an SV and upgrade the RV */
    (void)sv_upgrade(hv, SVt_PVHV); /* needs to be an HV type */

#ifndef NODEFAULT_SHAREKEYS
    HvSHAREKEYS_on(hv);         /* key-sharing on by default */
#endif

    while (isSPACE(*s)) /* leading whitespace is OK */
	s++;

    if (*s == 'v') {
	s++;  /* get past 'v' */
	qv = 1; /* force quoted version processing */
    }

    start = last = pos = s;

    /* pre-scan the input string to check for decimals/underbars */
    while ( *pos == '.' || *pos == '_' || isDIGIT(*pos) )
    {
	if ( *pos == '.' )
	{
	    if ( alpha )
		Perl_croak(aTHX_ "Invalid version format (underscores before decimal)");
	    saw_period++ ;
	    last = pos;
	}
	else if ( *pos == '_' )
	{
	    if ( alpha )
		Perl_croak(aTHX_ "Invalid version format (multiple underscores)");
	    alpha = 1;
	    width = pos - last - 1; /* natural width of sub-version */
	}
	pos++;
    }

    if ( alpha && !saw_period )
	Perl_croak(aTHX_ "Invalid version format (alpha without decimal)");

    if ( saw_period > 1 )
	qv = 1; /* force quoted version processing */

    pos = s;

    if ( qv )
	hv_store((HV *)hv, "qv", 2, newSViv(qv), 0);
    if ( alpha )
	hv_store((HV *)hv, "alpha", 5, newSViv(alpha), 0);
    if ( !qv && width < 3 )
	hv_store((HV *)hv, "width", 5, newSViv(width), 0);
    
    while (isDIGIT(*pos))
	pos++;
    if (!isALPHA(*pos)) {
	I32 rev;

	for (;;) {
	    rev = 0;
	    {
  		/* this is atoi() that delimits on underscores */
  		const char *end = pos;
  		I32 mult = 1;
 		I32 orev;

		/* the following if() will only be true after the decimal
		 * point of a version originally created with a bare
		 * floating point number, i.e. not quoted in any way
		 */
 		if ( !qv && s > start && saw_period == 1 ) {
		    mult *= 100;
 		    while ( s < end ) {
 			orev = rev;
 			rev += (*s - '0') * mult;
 			mult /= 10;
 			if ( PERL_ABS(orev) > PERL_ABS(rev) )
 			    Perl_croak(aTHX_ "Integer overflow in version");
 			s++;
			if ( *s == '_' )
			    s++;
 		    }
  		}
 		else {
 		    while (--end >= s) {
 			orev = rev;
 			rev += (*end - '0') * mult;
 			mult *= 10;
 			if ( PERL_ABS(orev) > PERL_ABS(rev) )
 			    Perl_croak(aTHX_ "Integer overflow in version");
 		    }
 		} 
  	    }

  	    /* Append revision */
	    av_push(av, newSViv(rev));
	    if ( *pos == '.' && isDIGIT(pos[1]) )
		s = ++pos;
	    else if ( *pos == '_' && isDIGIT(pos[1]) )
		s = ++pos;
	    else if ( isDIGIT(*pos) )
		s = pos;
	    else {
		s = pos;
		break;
	    }
	    if ( qv ) {
		while ( isDIGIT(*pos) )
		    pos++;
	    }
	    else {
		int digits = 0;
		while ( ( isDIGIT(*pos) || *pos == '_' ) && digits < 3 ) {
		    if ( *pos != '_' )
			digits++;
		    pos++;
		}
	    }
	}
    }
    if ( qv ) { /* quoted versions always get at least three terms*/
	I32 len = av_len(av);
	/* This for loop appears to trigger a compiler bug on OS X, as it
	   loops infinitely. Yes, len is negative. No, it makes no sense.
	   Compiler in question is:
	   gcc version 3.3 20030304 (Apple Computer, Inc. build 1640)
	   for ( len = 2 - len; len > 0; len-- )
	   av_push((AV *)sv, newSViv(0));
	*/
	len = 2 - len;
	while (len-- > 0)
	    av_push(av, newSViv(0));
    }

    if ( av_len(av) == -1 ) /* oops, someone forgot to pass a value */
	av_push(av, newSViv(0));

    /* And finally, store the AV in the hash */
    hv_store((HV *)hv, "version", 7, newRV_noinc((SV *)av), 0);
    return s;
}

/*
=for apidoc new_version

Returns a new version object based on the passed in SV:

    SV *sv = new_version(SV *ver);

Does not alter the passed in ver SV.  See "upg_version" if you
want to upgrade the SV.

=cut
*/

SV *
Perl_new_version(pTHX_ SV *ver)
{
    SV * const rv = newSV(0);
    if ( sv_derived_from(ver,"version") ) /* can just copy directly */
    {
	I32 key;
	AV * const av = newAV();
	AV *sav;
	/* This will get reblessed later if a derived class*/
	SV * const hv = newSVrv(rv, "version"); 
	(void)sv_upgrade(hv, SVt_PVHV); /* needs to be an HV type */
#ifndef NODEFAULT_SHAREKEYS
	HvSHAREKEYS_on(hv);         /* key-sharing on by default */
#endif

	if ( SvROK(ver) )
	    ver = SvRV(ver);

	/* Begin copying all of the elements */
	if ( hv_exists((HV *)ver, "qv", 2) )
	    hv_store((HV *)hv, "qv", 2, &PL_sv_yes, 0);

	if ( hv_exists((HV *)ver, "alpha", 5) )
	    hv_store((HV *)hv, "alpha", 5, &PL_sv_yes, 0);
	
	if ( hv_exists((HV*)ver, "width", 5 ) )
	{
	    const I32 width = SvIV(*hv_fetch((HV*)ver, "width", 5, FALSE));
	    hv_store((HV *)hv, "width", 5, newSViv(width), 0);
	}

	sav = (AV *)SvRV(*hv_fetch((HV*)ver, "version", 7, FALSE));
	/* This will get reblessed later if a derived class*/
	for ( key = 0; key <= av_len(sav); key++ )
	{
	    const I32 rev = SvIV(*av_fetch(sav, key, FALSE));
	    av_push(av, newSViv(rev));
	}

	hv_store((HV *)hv, "version", 7, newRV_noinc((SV *)av), 0);
	return rv;
    }
#ifdef SvVOK
    if ( SvVOK(ver) ) { /* already a v-string */
	const MAGIC* const mg = mg_find(ver,PERL_MAGIC_vstring);
	const STRLEN len = mg->mg_len;
	char * const version = savepvn( (const char*)mg->mg_ptr, len);
	sv_setpvn(rv,version,len);
	Safefree(version);
    }
    else {
#endif
    sv_setsv(rv,ver); /* make a duplicate */
#ifdef SvVOK
    }
#endif
    upg_version(rv);
    return rv;
}

/*
=for apidoc upg_version

In-place upgrade of the supplied SV to a version object.

    SV *sv = upg_version(SV *sv);

Returns a pointer to the upgraded SV.

=cut
*/

SV *
Perl_upg_version(pTHX_ SV *ver)
{
    const char *version, *s;
    bool qv = 0;

    if ( SvNOK(ver) ) /* may get too much accuracy */ 
    {
	char tbuf[64];
	sprintf(tbuf,"%.9"NVgf, SvNVX(ver));
	version = savepv(tbuf);
    }
#ifdef SvVOK
    else if ( SvVOK(ver) ) { /* already a v-string */
	const MAGIC* const mg = mg_find(ver,PERL_MAGIC_vstring);
	version = savepvn( (const char*)mg->mg_ptr,mg->mg_len );
	qv = 1;
    }
#endif
    else /* must be a string or something like a string */
    {
	version = savepv(SvPV_nolen(ver));
    }
    s = scan_version(version, ver, qv);
    if ( *s != '\0' )
	if(ckWARN(WARN_MISC))
	    Perl_warner(aTHX_ packWARN(WARN_MISC),
	      "Version string '%s' contains invalid data; "
	      "ignoring: '%s'", version, s);
    Safefree(version);
    return ver;
}

/*
=for apidoc vverify

Validates that the SV contains a valid version object.

    bool vverify(SV *vobj);

Note that it only confirms the bare minimum structure (so as not to get
confused by derived classes which may contain additional hash entries):

=over 4

=item * The SV contains a [reference to a] hash

=item * The hash contains a "version" key

=item * The "version" key has [a reference to] an AV as its value

=back

=cut
*/

bool
Perl_vverify(pTHX_ SV *vs)
{
    SV *sv;
    if ( SvROK(vs) )
	vs = SvRV(vs);

    /* see if the appropriate elements exist */
    if ( SvTYPE(vs) == SVt_PVHV
	 && hv_exists((HV*)vs, "version", 7)
	 && (sv = SvRV(*hv_fetch((HV*)vs, "version", 7, FALSE)))
	 && SvTYPE(sv) == SVt_PVAV )
	return TRUE;
    else
	return FALSE;
}

/*
=for apidoc vnumify

Accepts a version object and returns the normalized floating
point representation.  Call like:

    sv = vnumify(rv);

NOTE: you can pass either the object directly or the SV
contained within the RV.

=cut
*/

SV *
Perl_vnumify(pTHX_ SV *vs)
{
    I32 i, len, digit;
    int width;
    bool alpha = FALSE;
    SV * const sv = newSV(0);
    AV *av;
    if ( SvROK(vs) )
	vs = SvRV(vs);

    if ( !vverify(vs) )
	Perl_croak(aTHX_ "Invalid version object");

    /* see if various flags exist */
    if ( hv_exists((HV*)vs, "alpha", 5 ) )
	alpha = TRUE;
    if ( hv_exists((HV*)vs, "width", 5 ) )
	width = SvIV(*hv_fetch((HV*)vs, "width", 5, FALSE));
    else
	width = 3;


    /* attempt to retrieve the version array */
    if ( !(av = (AV *)SvRV(*hv_fetch((HV*)vs, "version", 7, FALSE)) ) ) {
	sv_catpvn(sv,"0",1);
	return sv;
    }

    len = av_len(av);
    if ( len == -1 )
    {
	sv_catpvn(sv,"0",1);
	return sv;
    }

    digit = SvIV(*av_fetch(av, 0, 0));
    Perl_sv_setpvf(aTHX_ sv, "%d.", (int)PERL_ABS(digit));
    for ( i = 1 ; i < len ; i++ )
    {
	digit = SvIV(*av_fetch(av, i, 0));
	if ( width < 3 ) {
	    const int denom = (width == 2 ? 10 : 100);
	    const div_t term = div((int)PERL_ABS(digit),denom);
	    Perl_sv_catpvf(aTHX_ sv, "%0*d_%d", width, term.quot, term.rem);
	}
	else {
	    Perl_sv_catpvf(aTHX_ sv, "%0*d", width, (int)digit);
	}
    }

    if ( len > 0 )
    {
	digit = SvIV(*av_fetch(av, len, 0));
	if ( alpha && width == 3 ) /* alpha version */
	    sv_catpvn(sv,"_",1);
	Perl_sv_catpvf(aTHX_ sv, "%0*d", width, (int)digit);
    }
    else /* len == 0 */
    {
	sv_catpvn(sv,"000",3);
    }
    return sv;
}

/*
=for apidoc vnormal

Accepts a version object and returns the normalized string
representation.  Call like:

    sv = vnormal(rv);

NOTE: you can pass either the object directly or the SV
contained within the RV.

=cut
*/

SV *
Perl_vnormal(pTHX_ SV *vs)
{
    I32 i, len, digit;
    bool alpha = FALSE;
    SV * const sv = newSV(0);
    AV *av;
    if ( SvROK(vs) )
	vs = SvRV(vs);

    if ( !vverify(vs) )
	Perl_croak(aTHX_ "Invalid version object");

    if ( hv_exists((HV*)vs, "alpha", 5 ) )
	alpha = TRUE;
    av = (AV *)SvRV(*hv_fetch((HV*)vs, "version", 7, FALSE));

    len = av_len(av);
    if ( len == -1 )
    {
	sv_catpvn(sv,"",0);
	return sv;
    }
    digit = SvIV(*av_fetch(av, 0, 0));
    Perl_sv_setpvf(aTHX_ sv, "v%"IVdf, (IV)digit);
    for ( i = 1 ; i < len ; i++ ) {
	digit = SvIV(*av_fetch(av, i, 0));
	Perl_sv_catpvf(aTHX_ sv, ".%"IVdf, (IV)digit);
    }

    if ( len > 0 )
    {
	/* handle last digit specially */
	digit = SvIV(*av_fetch(av, len, 0));
	if ( alpha )
	    Perl_sv_catpvf(aTHX_ sv, "_%"IVdf, (IV)digit);
	else
	    Perl_sv_catpvf(aTHX_ sv, ".%"IVdf, (IV)digit);
    }

    if ( len <= 2 ) { /* short version, must be at least three */
	for ( len = 2 - len; len != 0; len-- )
	    sv_catpvn(sv,".0",2);
    }
    return sv;
}

/*
=for apidoc vstringify

In order to maintain maximum compatibility with earlier versions
of Perl, this function will return either the floating point
notation or the multiple dotted notation, depending on whether
the original version contained 1 or more dots, respectively

=cut
*/

SV *
Perl_vstringify(pTHX_ SV *vs)
{
    if ( SvROK(vs) )
	vs = SvRV(vs);
    
    if ( !vverify(vs) )
	Perl_croak(aTHX_ "Invalid version object");

    if ( hv_exists((HV *)vs, "qv", 2) )
	return vnormal(vs);
    else
	return vnumify(vs);
}

/*
=for apidoc vcmp

Version object aware cmp.  Both operands must already have been 
converted into version objects.

=cut
*/

int
Perl_vcmp(pTHX_ SV *lhv, SV *rhv)
{
    I32 i,l,m,r,retval;
    bool lalpha = FALSE;
    bool ralpha = FALSE;
    I32 left = 0;
    I32 right = 0;
    AV *lav, *rav;
    if ( SvROK(lhv) )
	lhv = SvRV(lhv);
    if ( SvROK(rhv) )
	rhv = SvRV(rhv);

    if ( !vverify(lhv) )
	Perl_croak(aTHX_ "Invalid version object");

    if ( !vverify(rhv) )
	Perl_croak(aTHX_ "Invalid version object");

    /* get the left hand term */
    lav = (AV *)SvRV(*hv_fetch((HV*)lhv, "version", 7, FALSE));
    if ( hv_exists((HV*)lhv, "alpha", 5 ) )
	lalpha = TRUE;

    /* and the right hand term */
    rav = (AV *)SvRV(*hv_fetch((HV*)rhv, "version", 7, FALSE));
    if ( hv_exists((HV*)rhv, "alpha", 5 ) )
	ralpha = TRUE;

    l = av_len(lav);
    r = av_len(rav);
    m = l < r ? l : r;
    retval = 0;
    i = 0;
    while ( i <= m && retval == 0 )
    {
	left  = SvIV(*av_fetch(lav,i,0));
	right = SvIV(*av_fetch(rav,i,0));
	if ( left < right  )
	    retval = -1;
	if ( left > right )
	    retval = +1;
	i++;
    }

    /* tiebreaker for alpha with identical terms */
    if ( retval == 0 && l == r && left == right && ( lalpha || ralpha ) )
    {
	if ( lalpha && !ralpha )
	{
	    retval = -1;
	}
	else if ( ralpha && !lalpha)
	{
	    retval = +1;
	}
    }

    if ( l != r && retval == 0 ) /* possible match except for trailing 0's */
    {
	if ( l < r )
	{
	    while ( i <= r && retval == 0 )
	    {
		if ( SvIV(*av_fetch(rav,i,0)) != 0 )
		    retval = -1; /* not a match after all */
		i++;
	    }
	}
	else
	{
	    while ( i <= l && retval == 0 )
	    {
		if ( SvIV(*av_fetch(lav,i,0)) != 0 )
		    retval = +1; /* not a match after all */
		i++;
	    }
	}
    }
    return retval;
}
