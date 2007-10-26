use strict;
use Test;

use Crypt::OpenSSL::RSA;

BEGIN { plan tests => 10 }

my $PRIVATE_KEY_STRING = <<EOF;
-----BEGIN RSA PRIVATE KEY-----
MBsCAQACAU0CAQcCASsCAQcCAQsCAQECAQMCAQI=
-----END RSA PRIVATE KEY-----
EOF

my $PUBLIC_KEY_PKCS1_STRING = <<EOF;
-----BEGIN RSA PUBLIC KEY-----
MAYCAU0CAQc=
-----END RSA PUBLIC KEY-----
EOF

my $PUBLIC_KEY_X509_STRING = <<EOF;
-----BEGIN PUBLIC KEY-----
MBowDQYJKoZIhvcNAQEBBQADCQAwBgIBTQIBBw==
-----END PUBLIC KEY-----
EOF

my ($private_key, $public_key);

ok($private_key = Crypt::OpenSSL::RSA->new_private_key($PRIVATE_KEY_STRING));
ok($PRIVATE_KEY_STRING eq $private_key->get_private_key_string());
ok($PUBLIC_KEY_PKCS1_STRING eq $private_key->get_public_key_string());
ok($PUBLIC_KEY_X509_STRING eq $private_key->get_public_key_x509_string());

ok($public_key = Crypt::OpenSSL::RSA->new_public_key($PUBLIC_KEY_PKCS1_STRING));
ok($PUBLIC_KEY_PKCS1_STRING eq $public_key->get_public_key_string());
ok($PUBLIC_KEY_X509_STRING eq $public_key->get_public_key_x509_string());

ok($public_key = Crypt::OpenSSL::RSA->new_public_key($PUBLIC_KEY_X509_STRING));
ok($PUBLIC_KEY_PKCS1_STRING eq $public_key->get_public_key_string());
ok($PUBLIC_KEY_X509_STRING eq $public_key->get_public_key_x509_string());
