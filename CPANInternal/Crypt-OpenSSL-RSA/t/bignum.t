use strict;
use Test;

use Crypt::OpenSSL::RSA;

my $bignum_missing;

BEGIN
{
    # FIXME - add version requirement
    eval { require Crypt::OpenSSL::Bignum; };
    $bignum_missing = $@;
    plan(tests => $bignum_missing ? 0 : 64);
}

sub check_datum
{
    my ($p_expected, $p_actual) = @_;
    ok(defined($p_expected)
        ? $p_actual && $p_expected->equals($p_actual)
        : ! defined($p_actual));
}

sub check_key_parameters # runs 8 tests
{
    my ($p_rsa, $n, $e, $d, $p, $q, $dmp1, $dmq1, $iqmp) = @_;
    my ($rn, $re, $rd, $rp, $rq, $rdmp1, $rdmq1, $riqmp) =
        $p_rsa->get_key_parameters();

    check_datum($n, $rn);
    check_datum($e, $re);
    check_datum($d, $rd);
    check_datum($p, $rp);
    check_datum($q, $rq);
    check_datum($dmp1, $rdmp1);
    check_datum($dmq1, $rdmq1);
    check_datum($iqmp, $riqmp);
}

unless ($bignum_missing)
{
    my $ctx = Crypt::OpenSSL::Bignum::CTX->new();
    my $one = Crypt::OpenSSL::Bignum->one();
    my $p = Crypt::OpenSSL::Bignum->new_from_word(65521);
    my $q = Crypt::OpenSSL::Bignum->new_from_word(65537);
    my $e = Crypt::OpenSSL::Bignum->new_from_word(11);
    my $d = $e->mod_inverse($p->sub($one)->mul($q->sub($one), $ctx), $ctx);
    my $n = $p->mul($q, $ctx);
    my $dmp1 = $d->mod($p->sub($one), $ctx);
    my $dmq1 = $d->mod($q->sub($one), $ctx);
    my $iqmp = $q->mod_inverse($p, $ctx);

    my $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters($n, $e, $d, $p, $q);
    ok($rsa);

    $rsa->use_no_padding();

    my $plaintext = pack('C*', 100, 100, 100, 12);
    my $ciphertext = Crypt::OpenSSL::Bignum->new_from_bin($plaintext)->
        mod_exp($e, $n, $ctx)->to_bin();
    check_key_parameters($rsa, $n, $e, $d, $p, $q, $dmp1, $dmq1, $iqmp);

    ok($rsa->encrypt($plaintext) eq $ciphertext);
    ok($rsa->decrypt($ciphertext) eq $plaintext);

    my $rsa_pub = Crypt::OpenSSL::RSA->
        new_public_key($rsa->get_public_key_string());

    $rsa_pub->use_no_padding();
    ok($rsa->private_encrypt($ciphertext) eq $plaintext);
    ok($rsa_pub->public_decrypt($plaintext) eq $ciphertext);

    my @pub_parameters = $rsa_pub->get_key_parameters();
    ok(scalar(@pub_parameters) == 8);

    check_key_parameters($rsa_pub, $n, $e);

    $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters($n, $e, $d, $p);
    check_key_parameters($rsa, $n, $e, $d, $p, $q, $dmp1, $dmq1, $iqmp);

    $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters
        ($n, $e, $d, undef, $q);
    check_key_parameters($rsa, $n, $e, $d, $p, $q, $dmp1, $dmq1, $iqmp);

    $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters($n, $e);
    check_key_parameters($rsa, $n, $e);

    $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters($n, $e, $d);
    check_key_parameters($rsa, $n, $e, $d);

    $rsa = Crypt::OpenSSL::RSA->new_key_from_parameters($n, $e, undef, $p);
    check_key_parameters($rsa, $n, $e, $d, $p, $q, $dmp1, $dmq1, $iqmp);

    eval
    {
        Crypt::OpenSSL::RSA->new_key_from_parameters
                ($n->sub(Crypt::OpenSSL::Bignum->one()),
                 $e, $d, undef, $q);
    };
    ok($@ =~ /OpenSSL error: p not prime/);

    #try again, to make sure the error queue was properly flushed
    eval
    {
        Crypt::OpenSSL::RSA->new_key_from_parameters
                ($n->sub(Crypt::OpenSSL::Bignum->one()),
                 $e, $d, undef, $q);
    };
    ok($@ =~ /OpenSSL error: p not prime/);
}
