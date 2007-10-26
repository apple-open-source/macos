use strict;
use Test;

use Crypt::OpenSSL::Random;
use Crypt::OpenSSL::RSA;

BEGIN { plan tests => 19 }

sub _Test_Sign_And_Verify
{
    my( $plaintext, $rsa, $rsa_pub ) = @_;

    my $sig = $rsa->sign($plaintext);
    ok( $rsa_pub->verify( $plaintext, $sig ) );

    my $false_sig = unpack "H*", $sig;
    $false_sig =~ tr/[a-f]/[0a-d]/;
    ok(! $rsa_pub->verify( $plaintext, pack( "H*", $false_sig ) ) );
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

ok( Crypt::OpenSSL::RSA->generate_key( 512 )->size() * 8 == 512 );

my $rsa = Crypt::OpenSSL::RSA->generate_key( 1024 );
ok( $rsa->size() * 8 == 1024 );
ok( $rsa->check_key() );

# check if NO_PADDING works
$rsa->use_no_padding();
my ($ciphertext, $decoded_text);
my $plaintext = "X" x $rsa->size;
ok($ciphertext = $rsa->encrypt($plaintext));
ok($decoded_text = $rsa->decrypt($ciphertext));
ok ($decoded_text eq $plaintext);

# check if OAEP_PADDING works
my $plaintext_length = $rsa->size() - 42;
$plaintext = pack("C$plaintext_length",
                     (255,0,128,4, # Make sure these characters work
                      map {int(rand 256)} (1..$plaintext_length-4)));
$rsa->use_pkcs1_oaep_padding();
ok($ciphertext = $rsa->encrypt($plaintext));
ok($decoded_text = $rsa->decrypt($ciphertext));
ok ($decoded_text eq $plaintext);

my $private_key_string = $rsa->get_private_key_string();
my $public_key_string = $rsa->get_public_key_string();

ok( $private_key_string and $public_key_string );

my $rsa_priv = Crypt::OpenSSL::RSA->new_private_key($private_key_string);
$decoded_text = $rsa_priv->decrypt($ciphertext);

ok( $decoded_text eq $plaintext );

my $rsa_pub = Crypt::OpenSSL::RSA->new_public_key( $public_key_string );

$ciphertext = $rsa_pub->encrypt($plaintext);
$decoded_text = $rsa->decrypt($ciphertext);
ok ($decoded_text eq $plaintext);

$plaintext .= $plaintext x 5;
# check signature algorithms
$rsa->use_md5_hash();
$rsa_pub->use_md5_hash();
_Test_Sign_And_Verify( $plaintext, $rsa, $rsa_pub );

$rsa->use_sha1_hash();
$rsa_pub->use_sha1_hash();
_Test_Sign_And_Verify( $plaintext, $rsa, $rsa_pub );

$rsa->use_ripemd160_hash();
$rsa_pub->use_ripemd160_hash();
_Test_Sign_And_Verify( $plaintext, $rsa, $rsa_pub );

# check subclassing

eval { Crypt::OpenSSL::RSA::Subpackage->generate_key( 256 ); };
ok( !$@ );

package Crypt::OpenSSL::RSA::Subpackage;

use base qw( Crypt::OpenSSL::RSA );
