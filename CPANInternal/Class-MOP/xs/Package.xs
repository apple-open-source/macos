#include "mop.h"

MODULE = Class::MOP::Package   PACKAGE = Class::MOP::Package

PROTOTYPES: DISABLE

void
get_all_package_symbols(self, filter=TYPE_FILTER_NONE)
    SV *self
    type_filter_t filter
    PREINIT:
        HV *stash = NULL;
        HV *symbols = NULL;
        register HE *he;
    PPCODE:
        if ( ! SvROK(self) ) {
            die("Cannot call get_all_package_symbols as a class method");
        }

        if (GIMME_V == G_VOID) {
            XSRETURN_EMPTY;
        }

        PUTBACK;

        if ( (he = hv_fetch_ent((HV *)SvRV(self), KEY_FOR(package), 0, HASH_FOR(package))) ) {
            stash = gv_stashsv(HeVAL(he), 0);
        }


        if (!stash) {
            XSRETURN_UNDEF;
        }

        symbols = mop_get_all_package_symbols(stash, filter);
        PUSHs(sv_2mortal(newRV_noinc((SV *)symbols)));

BOOT:
    INSTALL_SIMPLE_READER_WITH_KEY(Package, name, package);
