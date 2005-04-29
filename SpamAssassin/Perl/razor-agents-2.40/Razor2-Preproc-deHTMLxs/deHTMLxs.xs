#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"



/* $Id: deHTMLxs.xs,v 1.1 2004/04/19 17:50:30 dasenbro Exp $ */

/* try to be compatible with older perls */
/* SvPV_nolen() macro first defined in 5.005_55 */
/* this is slow, not threadsafe, but works */
#include "patchlevel.h"
#if (PATCHLEVEL == 4) || ((PATCHLEVEL == 5) && (SUBVERSION < 55))
static STRLEN nolen_na;
# define SvPV_nolen(sv) SvPV ((sv), nolen_na)
#endif

#include "deHTMLxs.h"
//#include <stdio>

typedef struct mystate {
  int is_xs;
} *Razor2__Preproc__deHTMLxs;

MODULE = Razor2::Preproc::deHTMLxs		PACKAGE = Razor2::Preproc::deHTMLxs		

PROTOTYPES: ENABLE


Razor2::Preproc::deHTMLxs
new(class)
    SV *    class
    CODE:
    {
        //bool Init(void)
            
        Newz(0, RETVAL, 1, struct mystate); 
        RETVAL->is_xs = 1;  /* placeholder, not used now */
          //# dprint(" -- Opening debug file: /tmp/nilsimsa_debug.txt"); 
    }
    OUTPUT:
        RETVAL

int
is_xs(self)
    Razor2::Preproc::deHTMLxs self;
    CODE:
        RETVAL = 1;
	OUTPUT:
        RETVAL

char *
testxs(self, str)
    Razor2::Preproc::deHTMLxs self;
    char *		str;
    CODE:
        RETVAL = str + 1;
	OUTPUT:
        RETVAL

SV *
isit(self, scalarref)
    Razor2::Preproc::deHTMLxs self;
    SV *	scalarref;
    CODE:
    {
        /* 2002/11/21 Anne Bennett: use the right type def: */
        STRLEN size;
        char * raw;
        SV *  text;
        const char mynull = 0;

        if (SvROK(scalarref)) {
            text = SvRV(scalarref);

            // normally perl has '\0' on end, but not guaranteed
            sv_catpv(text,&mynull);
            raw = SvPV(text,size);

        //fprintf(stderr, "deHTMLxs::isit(%d) strlen=%d\n", SvCUR(text), strlen(raw));

            //  bool is_html(const char *);
            if (is_html(raw)) {
                RETVAL = newSVpv ("1", 0);
            } else {
                RETVAL = newSVpv ("", 0);
                //RETVAL = newSVsv(&sv_undef);
            }
        } else {
            //fprintf(stderr, "deHTMLxs::isit() got invalid arg - not a scalar ref\n");
                RETVAL = newSVpv ("", 0);
        }
        //fprintf(stderr, "deHTMLxs::isit() done\n");
    }
    OUTPUT:
        RETVAL

SV *
doit(self, scalarref)
    Razor2::Preproc::deHTMLxs self;
    SV *	scalarref
    CODE:
    {
        char * cleaned, * raw, * res;
        /* 2002/11/21 Anne Bennett: use the right type def: */
        STRLEN size;
        SV *	text;
        SV *	newtext;
        SV *	newref;

        if (SvROK(scalarref)) {
            text = SvRV(scalarref);
            raw = SvPV(text,size);

            if ( (cleaned = malloc(size+1)) && 
                 (res = html_strip(raw, cleaned))  // html_strip will memset cleaned to 0
                 ) {
                
                //
                // hook it up so scalarref will dereference to new scalar
                //

                newtext = newSVpv (res, 0);
                // SvREFCNT_inc(newtext);

                // newtext is new scalar containing cleaned html.
                // we want scalarref to point to that instead of its old dude, text.

                //  sv_setsv (SV* dest, SV* src)
                sv_setsv(text, newtext);  
                //SvREFCNT_inc(text);
                SvREFCNT_inc(scalarref);

                RETVAL = scalarref;
                free(cleaned);

            } else {
                if (cleaned) {
                    //fprintf(stderr, "deHTMLxs::doit() html_strip failed.\n");
                    free(cleaned);
                } else {
                    //fprintf(stderr, "deHTMLxs::doit() malloc(%d) failed.\n", size+1 );
                }

                RETVAL = newSVpv ("", 0);
                //RETVAL = newSVsv(&sv_undef);
            }
        } else {
            //fprintf(stderr, "deHTMLxs::isit() got invalid arg - not scalar\n");
                RETVAL = newSVpv ("", 0);
        }
        //fprintf(stderr, "deHTMLxs::doit() done\n");

    }
    OUTPUT:
        RETVAL







