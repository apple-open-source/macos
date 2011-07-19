
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

/*
get_code_info:
  Pass in a coderef, returns:
  [ $pkg_name, $coderef_name ] ie:
  [ 'Foo::Bar', 'new' ]
*/

MODULE = Sub::Identify   PACKAGE = Sub::Identify

PROTOTYPES: ENABLE

void
get_code_info(coderef)
    SV* coderef
    PREINIT:
        char* name;
        char* pkg;
    PPCODE:
        if (SvOK(coderef) && SvROK(coderef) && SvTYPE(SvRV(coderef)) == SVt_PVCV) {
            coderef = SvRV(coderef);
            if (CvGV(coderef)) {
                name = GvNAME( CvGV(coderef) );
                pkg = HvNAME( GvSTASH(CvGV(coderef)) );
                EXTEND(SP, 2);
                PUSHs(sv_2mortal(newSVpvn(pkg, strlen(pkg))));
                PUSHs(sv_2mortal(newSVpvn(name, strlen(name))));
            }
            else {
                /* sub is being compiled: bail out and return nothing. */
            }
        }
