#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define NEED_eval_pv
#define NEED_grok_hex
#define NEED_grok_number
#define NEED_grok_numeric_radix
#define NEED_grok_oct
#define NEED_newRV_noinc
#define NEED_newSVpvn_share
#define NEED_sv_2pv_flags

#include "ppport.h"
#include "ppport_math.h"
#include "ppport_sort.h"

#ifndef is_utf8_string
#define is_utf8_string(x, y) (0==1)
#endif

#undef DEBUG /* maybe defined in perl.h */
#include <syck.h>

#ifndef newSVpvn_share
#define newSVpvn_share(x, y, z) newSVpvn(x, y)
#endif

/*
#undef ASSERT
#include "Storable.xs"
*/

struct emitter_xtra {
    SV* port;
    char* tag;
    char dump_code;
    bool implicit_binary;
};

struct parser_xtra {
    AV *objects;
    bool implicit_unicode;
    bool load_code;
};

SV* perl_syck_lookup_sym( SyckParser *p, SYMID v) {
    /* Not "undef" becase otherwise we have a warning on self-recursive nodes */
    SV *obj = &PL_sv_no;
    syck_lookup_sym(p, v, (char **)&obj);
    return obj;
}

#ifdef SvUTF8_on
#define CHECK_UTF8 \
    if (((struct parser_xtra *)p->bonus)->implicit_unicode \
      && is_utf8_string((U8*)n->data.str->ptr, n->data.str->len)) \
        SvUTF8_on(sv);
#else
#define CHECK_UTF8 ;
#endif

SyckNode * perl_syck_bad_anchor_handler(SyckParser *p, char *a) {
    SyckNode *badanc = syck_new_map(
        (SYMID)newSVpvn_share("name", 4, 0),
        (SYMID)newSVpvn_share(a, strlen(a), 0)
    );
    badanc->type_id = syck_strndup( "!perl:YAML::Syck::BadAlias", 25 );
    return badanc;
}

void perl_syck_error_handler(SyckParser *p, char *msg) {
    croak(form( "%s parser (line %d, column %d): %s", 
        "Syck",
        p->linect + 1,
        p->cursor - p->lineptr,
        msg ));
}

void perl_syck_output_handler(SyckEmitter *e, char *str, long len) {
    struct emitter_xtra *bonus = (struct emitter_xtra *)e->bonus;
    sv_catpvn_nomg(bonus->port, str, len);
}

