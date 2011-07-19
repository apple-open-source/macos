#include "mop.h"

SV *mop_method_metaclass;
SV *mop_associated_metaclass;
SV *mop_wrap;

static void
mop_update_method_map(pTHX_ SV *const self, SV *const class_name, HV *const stash, HV *const map)
{
    const char *const class_name_pv = HvNAME(stash); /* must be HvNAME(stash), not SvPV_nolen_const(class_name) */
    SV   *method_metaclass_name;
    char *method_name;
    I32   method_name_len;
    SV   *coderef;
    HV   *symbols;
    dSP;

    symbols = mop_get_all_package_symbols(stash, TYPE_FILTER_CODE);
    sv_2mortal((SV*)symbols);
    (void)hv_iterinit(symbols);
    while ( (coderef = hv_iternextsv(symbols, &method_name, &method_name_len)) ) {
        CV *cv = (CV *)SvRV(coderef);
        char *cvpkg_name;
        char *cv_name;
        SV *method_slot;
        SV *method_object;

        if (!mop_get_code_info(coderef, &cvpkg_name, &cv_name)) {
            continue;
        }

        /* this checks to see that the subroutine is actually from our package  */
        if ( !(strEQ(cvpkg_name, "constant") && strEQ(cv_name, "__ANON__")) ) {
            if ( strNE(cvpkg_name, class_name_pv) ) {
                continue;
            }
        }

        method_slot = *hv_fetch(map, method_name, method_name_len, TRUE);
        if ( SvOK(method_slot) ) {
            SV *body;

            if ( sv_isobject(method_slot) ) {
                body = mop_call0(aTHX_ method_slot, KEY_FOR(body)); /* $method_object->body() */
            }
            else {
                body = method_slot;
            }

            if ( SvROK(body) && ((CV *) SvRV(body)) == cv ) {
                continue;
            }
        }

        method_metaclass_name = mop_call0(aTHX_ self, mop_method_metaclass); /* $self->method_metaclass() */

        /*
            $method_object = $method_metaclass->wrap(
                $cv,
                associated_metaclass => $self,
                package_name         => $class_name,
                name                 => $method_name
            );
        */
        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 8);
        PUSHs(method_metaclass_name); /* invocant */
        mPUSHs(newRV_inc((SV *)cv));
        PUSHs(mop_associated_metaclass);
        PUSHs(self);
        PUSHs(KEY_FOR(package_name));
        PUSHs(class_name);
        PUSHs(KEY_FOR(name));
        mPUSHs(newSVpv(method_name, method_name_len));
        PUTBACK;

        call_sv(mop_wrap, G_SCALAR | G_METHOD);
        SPAGAIN;
        method_object = POPs;
        PUTBACK;
        /* $map->{$method_name} = $method_object */
        sv_setsv(method_slot, method_object);

        FREETMPS;
        LEAVE;
    }
}

MODULE = Class::MOP::Mixin::HasMethods   PACKAGE = Class::MOP::Mixin::HasMethods

PROTOTYPES: DISABLE

void
_full_method_map(self)
    SV *self
    PREINIT:
        HV *const obj        = (HV *)SvRV(self);
        SV *const class_name = HeVAL( hv_fetch_ent(obj, KEY_FOR(package), 0, HASH_FOR(package)) );
        HV *const stash      = gv_stashsv(class_name, 0);
        UV current;
        SV *cache_flag;
        SV *map_ref;
    PPCODE:
        if (!stash) {
             mXPUSHs(newRV_noinc((SV *)newHV()));
             return;
        }

        current    = mop_check_package_cache_flag(aTHX_ stash);
        cache_flag = HeVAL( hv_fetch_ent(obj, KEY_FOR(package_cache_flag), TRUE, HASH_FOR(package_cache_flag)));
        map_ref    = HeVAL( hv_fetch_ent(obj, KEY_FOR(methods), TRUE, HASH_FOR(methods)));

        /* $self->{methods} does not yet exist (or got deleted) */
        if ( !SvROK(map_ref) || SvTYPE(SvRV(map_ref)) != SVt_PVHV ) {
            SV *new_map_ref = newRV_noinc((SV *)newHV());
            sv_2mortal(new_map_ref);
            sv_setsv(map_ref, new_map_ref);
        }

        if ( !SvOK(cache_flag) || SvUV(cache_flag) != current ) {
            mop_update_method_map(aTHX_ self, class_name, stash, (HV *)SvRV(map_ref));
            sv_setuv(cache_flag, mop_check_package_cache_flag(aTHX_ stash)); /* update_cache_flag() */
        }

        XPUSHs(map_ref);

BOOT:
    mop_method_metaclass     = newSVpvs("method_metaclass");
    mop_associated_metaclass = newSVpvs("associated_metaclass");
    mop_wrap                 = newSVpvs("wrap");
