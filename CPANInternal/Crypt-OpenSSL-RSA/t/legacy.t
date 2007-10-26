use strict;
use Test;

use Crypt::OpenSSL::Random;
use Crypt::OpenSSL::RSA;

BEGIN { plan tests => 11 }

#suppress deprecation warnings

sub no_warn
{
    my $sub = shift;
    my $warn = $SIG{__WARN__};
    $SIG{__WARN__} = sub { warn( @_ ) unless $_[0] =~ m/deprecated/i };
    my $return = &$sub();
    $SIG{__WARN__} = $warn;
    return $return;
}


# On platforms without a /dev/random, we need to manually seed.  In
# real life, the following would stink, but for testing purposes, it
# suffices to seed with any old thing, even if it is not actually
# random.  We'll at least emulate seeding from Crypt::OpenSSL::Random,
# which is what we would have to do in "real life", since the private
# data used by the OpenSSL random library apparently does not span
# across perl XS modules.

Crypt::OpenSSL::Random::random_seed("Here are 20 bytes...");
Crypt::OpenSSL::RSA->import_random_seed();

my $rsa = no_warn( sub { Crypt::OpenSSL::RSA->new() } );

no_warn( sub { $rsa->generate_key(512) } );
ok( $rsa->size() * 8 == 512 );

no_warn( sub { $rsa->generate_key(1024) } );
ok( $rsa->size() * 8 == 1024 );
ok( $rsa->check_key() );

my ($ciphertext, $decoded_text);
$rsa->use_no_padding();
my $plaintext = "X" x $rsa->size;
ok( $ciphertext = $rsa->encrypt($plaintext) );
ok( $decoded_text = $rsa->decrypt($ciphertext) );
ok( $decoded_text eq $plaintext );

my $private_key_string = $rsa->get_private_key_string();
my $public_key_string = $rsa->get_public_key_string();

ok( $private_key_string and $public_key_string );

my $rsa_priv = no_warn( sub{ Crypt::OpenSSL::RSA->new(); } );
$rsa_priv->use_no_padding();
$rsa_priv->load_private_key( $private_key_string );
$decoded_text = $rsa_priv->decrypt($ciphertext);

ok( $decoded_text eq $plaintext );

my $rsa_pub = no_warn( sub{ Crypt::OpenSSL::RSA->new(); } );

$rsa_pub->load_public_key($public_key_string);
$rsa_pub->use_no_padding();

$ciphertext = $rsa_pub->encrypt($plaintext);
$decoded_text = $rsa->decrypt($ciphertext);
ok ($decoded_text eq $plaintext);

# check subclassing

eval
{
    no_warn
        ( sub { Crypt::OpenSSL::RSA::Subpackage->new()->generate_key(256); } )
};
ok( !$@ );

eval { no_warn( sub { Crypt::OpenSSL::RSA::generate_key( {}, 256 ); } ) };
ok( $@ );

package Crypt::OpenSSL::RSA::Subpackage;

use base qw( Crypt::OpenSSL::RSA );
