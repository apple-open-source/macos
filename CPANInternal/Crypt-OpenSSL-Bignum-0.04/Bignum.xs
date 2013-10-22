#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <openssl/ssl.h>
#include <openssl/bn.h>

#define checkOpenSslCall( result ) if( ! ( result ) ) \
  croak( "OpenSSL error: %s", ERR_reason_error_string( ERR_get_error() ) );

SV* new_obj( SV * p_proto, void* obj )
{
    return sv_2mortal( sv_bless( newRV_noinc( newSViv( (IV)obj ) ),
                                 ( SvROK( p_proto )
                                   ? SvSTASH( SvRV( p_proto ) )
                                   : gv_stashsv( p_proto, 1 ) ) ) );
}

#define proto_obj( obj ) new_obj( ST(0), obj )

BIGNUM* sv2bn( SV* sv )
{
    if( ! SvROK( sv ) )
    {
      croak( "argument is not a Crypt::OpenSSL::Bignum object" );
    }
    return (BIGNUM*) SvIV( SvRV( sv ) );
}

MODULE = Crypt::OpenSSL::Bignum      PACKAGE = Crypt::OpenSSL::Bignum   PREFIX=BN_

BOOT:
    ERR_load_crypto_strings();

void
_free_BN(self)
    BIGNUM* self;
  CODE:
    BN_clear_free( self );

BIGNUM*
new_from_word(p_proto, p_word)
    SV* p_proto;
    unsigned long p_word;
  PREINIT:
    BIGNUM* bn;
  CODE:
    checkOpenSslCall( bn = BN_new() );
    checkOpenSslCall( BN_set_word( bn, p_word ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
new_from_decimal(p_proto, p_dec_string)
    SV* p_proto;
    char* p_dec_string;
  PREINIT:
    BIGNUM* bn;
  CODE:
    bn = NULL;
    checkOpenSslCall( BN_dec2bn( &bn, p_dec_string ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
new_from_hex(p_proto, p_hex_string)
    SV* p_proto;
    char* p_hex_string;
  PREINIT:
    BIGNUM* bn;
  CODE:
    bn = NULL;
    checkOpenSslCall( BN_hex2bn( &bn, p_hex_string ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
new_from_bin(p_proto, p_bin_string_SV)
    SV* p_proto;
    SV* p_bin_string_SV;
  PREINIT:
    BIGNUM* bn;
    char* bin;
    STRLEN bin_length;
  CODE:
    bin = SvPV( p_bin_string_SV, bin_length );
    checkOpenSslCall( bn = BN_bin2bn( bin, bin_length, NULL ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
zero(p_proto)
    SV* p_proto;
  PREINIT:
    BIGNUM *bn;
  CODE:
    checkOpenSslCall( bn = BN_new() );
    checkOpenSslCall( BN_zero( bn ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
one(p_proto)
    SV* p_proto;
  PREINIT:
    BIGNUM *bn;
  CODE:
    checkOpenSslCall( bn = BN_new() );
    checkOpenSslCall( BN_one( bn ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL



char*
to_decimal(self)
    BIGNUM* self;
  CODE:
    checkOpenSslCall( RETVAL = BN_bn2dec( self ) );
  OUTPUT:
    RETVAL
  CLEANUP:
    OPENSSL_free( RETVAL );


char*
to_hex(self)
    BIGNUM* self;
  CODE:
    checkOpenSslCall( RETVAL = BN_bn2hex( self ) );
  OUTPUT:
    RETVAL
  CLEANUP:
    OPENSSL_free( RETVAL );

SV*
to_bin(self)
    BIGNUM* self;
  PREINIT:
    char* bin;
    int length;
  CODE:
    length = BN_num_bytes( self );
    New( 0, bin, length, char );
    BN_bn2bin( self, bin );
    RETVAL = newSVpv( bin, length );
    Safefree( bin );
  OUTPUT:
    RETVAL

unsigned long
BN_get_word(self)
    BIGNUM* self;

PROTOTYPES: DISABLE

SV*
add(a, b, ...)
    BIGNUM* a;
    BIGNUM* b;
  PREINIT:
    BIGNUM *bn;
  PPCODE:
    if( items > 3 )
      croak( "usage: $bn->add( $bn2[, $target] )" );
    bn = ( items < 3 ) ? BN_new() : sv2bn( ST(2) );
    checkOpenSslCall( BN_add( bn, a, b ) );
    ST(0) = ( (items < 3 ) ? proto_obj( bn ) : ST(2) );
    XSRETURN(1);

SV*
sub(a, b, ...)
    BIGNUM* a;
    BIGNUM* b;
  PREINIT:
    BIGNUM *bn;
  PPCODE:
    if( items > 3 )
      croak( "usage: $bn->sub( $bn2[, $target] )" );
    bn = ( items < 3 ) ? BN_new() : sv2bn( ST(2) );
    checkOpenSslCall( BN_sub( bn, a, b ) );
    ST(0) = ( (items < 3 ) ? proto_obj( bn ) : ST(2) );
    XSRETURN(1);

SV*
mul(a, b, ctx, ...)
    BIGNUM* a;
    BIGNUM* b;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
  PPCODE:
    if( items > 4 )
      croak( "usage: $bn->mul( $bn2, $ctx, [, $target] )" );
    bn = ( items < 4 ) ? BN_new() : sv2bn( ST(3) );
    checkOpenSslCall( BN_mul( bn, a, b, ctx ) );
    ST(0) = ( (items < 4 ) ? proto_obj( bn ) : ST(3) );
    XSRETURN(1);

SV*
div(a, b, ctx, ...)
    BIGNUM* a;
    BIGNUM* b;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* quotient;
    BIGNUM* remainder;
  PPCODE:
    if( items > 5 )
      croak( "usage: $bn->add( $bn2, $ctx, [, $quotient [, $remainder ] ] )" );
    quotient = ( items < 4 ) ? BN_new() : sv2bn( ST(3) );
    remainder = ( items < 5 ) ? BN_new() : sv2bn( ST(4) );
    checkOpenSslCall( BN_div( quotient, remainder, a, b, ctx ) );
    ST(0) = ( (items < 4 ) ? proto_obj( quotient ) : ST(3) );
    ST(1) = ( (items < 5 ) ? proto_obj( remainder ) : ST(4) );
    XSRETURN(2);

BIGNUM*
sqr(a, ctx)
    BIGNUM* a;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_sqr( bn, a, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

SV*
mod(a, b, ctx, ...)
    BIGNUM* a;
    BIGNUM* b;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
  PPCODE:
    if( items > 4 )
      croak( "usage: $bn->add( $bn2, $ctx, [, $target] )" );
    bn = ( items < 4 ) ? BN_new() : sv2bn( ST(3) );
    checkOpenSslCall( BN_mod( bn, a, b, ctx ) );
    ST(0) = ( (items < 4 ) ? proto_obj( bn ) : ST(3) );
    XSRETURN(1);

BIGNUM*
mod_mul(a, b, m, ctx)
    BIGNUM* a;
    BIGNUM* b;
    BIGNUM* m;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_mod_mul( bn, a, b, m, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
exp(a, p, ctx)
    BIGNUM* a;
    BIGNUM* p;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_exp( bn, a, p, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
mod_exp(a, p, m, ctx)
    BIGNUM* a;
    BIGNUM* p;
    BIGNUM* m;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_mod_exp( bn, a, p, m, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
mod_inverse(a, n, ctx)
    BIGNUM* a;
    BIGNUM* n;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_mod_inverse( bn, a, n, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

BIGNUM*
gcd(a, b, ctx)
    BIGNUM* a;
    BIGNUM* b;
    BN_CTX* ctx;
  PREINIT:
    BIGNUM* bn;
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    bn = BN_new();
    checkOpenSslCall( BN_gcd( bn, a, b, ctx ) );
    RETVAL = bn;
  OUTPUT:
    RETVAL

int
BN_cmp(a, b)
    BIGNUM* a;
    BIGNUM* b;

int
BN_is_zero(a)
    BIGNUM* a;

int
BN_is_one(a)
    BIGNUM* a;

int
BN_is_odd(a)
    BIGNUM* a;

BIGNUM*
copy(a)
    BIGNUM* a;
  PREINIT:
    SV* p_proto;
  CODE:
    p_proto = ST(0);
    checkOpenSslCall( RETVAL = BN_dup(a) );
  OUTPUT:
    RETVAL

IV
pointer_copy(a)
    BIGNUM* a;
  PREINIT:
  CODE:
    checkOpenSslCall( RETVAL = (IV) BN_dup(a) );
  OUTPUT:
    RETVAL

MODULE = Crypt::OpenSSL::Bignum  PACKAGE = Crypt::OpenSSL::Bignum::CTX PREFIX=BN_CTX_

BN_CTX*
BN_CTX_new(p_proto)
    SV* p_proto;
  C_ARGS:

void
_free_BN_CTX(self)
    BN_CTX* self;
  CODE:
    BN_CTX_free( self );
