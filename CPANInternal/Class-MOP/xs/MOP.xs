#include "mop.h"

static bool
find_method (const char *key, STRLEN keylen, SV *val, void *ud)
{
    bool *found_method = (bool *)ud;
    PERL_UNUSED_ARG(key);
    PERL_UNUSED_ARG(keylen);
    PERL_UNUSED_ARG(val);
    *found_method = TRUE;
    return FALSE;
}

EXTERN_C XS(boot_Class__MOP__Mixin__HasMethods);
EXTERN_C XS(boot_Class__MOP__Package);
EXTERN_C XS(boot_Class__MOP__Mixin__AttributeCore);
EXTERN_C XS(boot_Class__MOP__Method);

MODULE = Class::MOP   PACKAGE = Class::MOP

PROTOTYPES: DISABLE

BOOT:
    mop_prehash_keys();

    MOP_CALL_BOOT (boot_Class__MOP__Mixin__HasMethods);
    MOP_CALL_BOOT (boot_Class__MOP__Package);
    MOP_CALL_BOOT (boot_Class__MOP__Mixin__AttributeCore);
    MOP_CALL_BOOT (boot_Class__MOP__Method);

# use prototype here to be compatible with get_code_info from Sub::Identify
void
get_code_info(coderef)
    SV *coderef
    PROTOTYPE: $
    PREINIT:
        char *pkg  = NULL;
        char *name = NULL;
    PPCODE:
        SvGETMAGIC(coderef);
        if (mop_get_code_info(coderef, &pkg, &name)) {
            EXTEND(SP, 2);
            mPUSHs(newSVpv(pkg, 0));
            mPUSHs(newSVpv(name, 0));
        }

void
is_class_loaded(klass)
    SV *klass
    PREINIT:
        HV *stash;
        bool found_method = FALSE;
    PPCODE:
        SvGETMAGIC(klass);
        if (!(SvPOKp(klass) && SvCUR(klass))) { /* XXX: SvPOK does not work with magical scalars */
            XSRETURN_NO;
        }

        stash = gv_stashsv(klass, 0);
        if (!stash) {
            XSRETURN_NO;
        }

        if (hv_exists_ent (stash, KEY_FOR(VERSION), HASH_FOR(VERSION))) {
            HE *version = hv_fetch_ent(stash, KEY_FOR(VERSION), 0, HASH_FOR(VERSION));
            SV *version_sv;
            if (version && HeVAL(version) && (version_sv = GvSV(HeVAL(version)))) {
                if (SvROK(version_sv)) {
                    SV *version_sv_ref = SvRV(version_sv);

                    if (SvOK(version_sv_ref)) {
                        XSRETURN_YES;
                    }
                }
                else if (SvOK(version_sv)) {
                    XSRETURN_YES;
                }
            }
        }

        if (hv_exists_ent (stash, KEY_FOR(ISA), HASH_FOR(ISA))) {
            HE *isa = hv_fetch_ent(stash, KEY_FOR(ISA), 0, HASH_FOR(ISA));
            if (isa && HeVAL(isa) && GvAV(HeVAL(isa)) && av_len(GvAV(HeVAL(isa))) != -1) {
                XSRETURN_YES;
            }
        }

        mop_get_package_symbols(stash, TYPE_FILTER_CODE, find_method, &found_method);
        if (found_method) {
            XSRETURN_YES;
        }

        XSRETURN_NO;
