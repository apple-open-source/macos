# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 52 };
use Crypt::OpenSSL::Bignum;
use Crypt::OpenSSL::Bignum::CTX;

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

use strict;

sub _new_bn
{
    return Crypt::OpenSSL::Bignum->new_from_word( shift );
}

my $bn;
my $decimal_string = "2342234235235235235";
$bn = Crypt::OpenSSL::Bignum->new_from_decimal( $decimal_string );
ok( $bn );
ok( $bn->to_decimal() eq $decimal_string );

my $hex_string = "7f";
$bn = Crypt::OpenSSL::Bignum->new_from_hex( $hex_string );
ok( $bn );
ok( $bn->to_hex() eq uc( $hex_string ) );
ok( $bn->to_decimal() eq '127' );

my $bin_string = pack( "C*", 2, 0 );
$bn = Crypt::OpenSSL::Bignum->new_from_bin( $bin_string );
ok( $bn );
ok( $bn->to_bin() eq $bin_string );
ok( $bn->to_decimal() eq '512' );

my $bn23 = _new_bn( 23 );
my $bn25 = _new_bn( 25 );

ok( $bn23->cmp($ bn25 ) == -1 );
ok( $bn25->cmp( $bn23 ) == 1 );
ok( $bn23->cmp( $bn23 ) == 0 );
ok( $bn23->equals( $bn23 ) );

my $bn_copy = $bn->copy();
ok( $bn_copy ne $bn );
ok( $bn->equals( $bn_copy ) );

my $ptr = $bn->pointer_copy();
ok( ! ref $ptr );
ok( $bn + 0 != $ptr );
my $from_ptr = Crypt::OpenSSL::Bignum->bless_pointer( $ptr );
ok( $bn->equals( $from_ptr ) );


my $zero = Crypt::OpenSSL::Bignum->zero();
my $one = Crypt::OpenSSL::Bignum->one();

ok( $one->is_one() );
ok( !$zero->is_one() );

ok( $zero->is_zero() );
ok( !$one->is_zero() );

ok( !$zero->is_odd() );
ok( $one->is_odd() );

my $word = 0xffffeeee;
ok( _new_bn($word)->get_word() == $word );

# test creation from object rather than class string.
my $bn2 = $bn->new_from_bin( $bin_string );
ok( $bn2 );
ok( $bn2->to_bin() eq $bn->to_bin() );

ok( '48' eq $bn23->add( $bn25 )->to_decimal() );
$bn = _new_bn( 18 );
$bn->add( $one, $bn );
ok( 19 == $bn->get_word() );

ok( '-2' eq $bn23->sub( $bn25 )->to_decimal() );
$bn = _new_bn( 18 );
$bn->sub( $one, $bn );
ok( 17 == $bn->get_word() );

my $ctx = Crypt::OpenSSL::Bignum::CTX->new();

ok( $ctx );
ok( 575 == $bn23->mul( $bn25, $ctx )->get_word() );
ok( 575 == $bn23->mul( $bn25, $ctx, $bn )->get_word() );
ok( 575 == $bn->get_word() );

ok( 2 == $bn25->mod( $bn23, $ctx )->get_word() );
ok( 2 == $bn25->mod( $bn23, $ctx, $bn )->get_word() );
ok( 2 == $bn->get_word() );

my $bn6 = _new_bn( 6 );
my $bn3 = _new_bn( 3 );

my( $quotient, $remainder ) = $bn25->div( $bn23, $ctx );
ok( $quotient->is_one );
ok( 2 == $remainder->get_word() );
my( $quotient2, $remainder2 ) =
    $bn25->div( $bn6, $ctx, $quotient, $remainder );
ok( $quotient2 == $quotient );
ok( $remainder2 == $remainder );
ok( 4 == $quotient->get_word() );
ok( $remainder->is_one );
my( $quotient3, $remainder3 ) =
    $bn25->div( $bn6, $ctx, $quotient );
ok( $quotient3 == $quotient );
ok( 4 == $quotient->get_word() );
ok( $remainder3->is_one() );

ok( 6 == _new_bn( 18 )->gcd( _new_bn( 42 ), $ctx )->get_word() );
ok( 5 == $bn23->mod_mul( $bn25, $bn6, $ctx )->get_word() );
ok( 729 == $bn3->exp( $bn6, $ctx )->get_word() );
ok( 4 == $bn3->mod_exp( $bn6, $bn25, $ctx )->get_word() );
ok( 36 == $bn6->sqr( $ctx )->get_word() );
ok( 12 == $bn23->mod_inverse( $bn25, $ctx )->get_word() );
