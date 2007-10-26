# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..1\n"; }
END {print "not ok 1\n" unless $loaded;}
use Crypt::OpenSSL::Random;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

my $test_number = 2;
sub my_test
{
    my($cond) = @_;
    my $number = $test_number++;
    if ($cond)
    {
        print "ok $number\n";
    }
    else
    {
        my ($pack, $file, $line) = caller;
        print "not ok $number - from $file:$line\n";
    }
}    

Crypt::OpenSSL::Random::random_seed("Here are 19 bytes...");

# We should now be seeded, regardless.
my_test(Crypt::OpenSSL::Random::random_status());

my_test(length(Crypt::OpenSSL::Random::random_bytes(53)), 53);
my_test(length(Crypt::OpenSSL::Random::random_pseudo_bytes(53)), 53);
