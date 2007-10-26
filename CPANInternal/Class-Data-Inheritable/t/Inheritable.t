# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use strict;

use vars qw($Total_tests);

my $loaded;
my $test_num = 1;
BEGIN { $| = 1; $^W = 1; }
END {print "not ok $test_num\n" unless $loaded;}
print "1..$Total_tests\n";
use Class::Data::Inheritable;
$loaded = 1;
ok(1, 'compile');
######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):
sub ok {
    my($test, $name) = @_;
    print "not " unless $test;
    print "ok $test_num";
    print " - $name" if defined $name;
    print "\n";
    $test_num++;
}

sub eqarray  {
    my($a1, $a2) = @_;
    return 0 unless @$a1 == @$a2;
    my $ok = 1;
    for (0..$#{$a1}) { 
        unless($a1->[$_] eq $a2->[$_]) {
        $ok = 0;
        last;
        }
    }
    return $ok;
}

# Change this to your # of ok() calls + 1
BEGIN { $Total_tests = 12 }

package Ray;
use base qw(Class::Data::Inheritable);

::ok( Ray->can('mk_classdata') );
Ray->mk_classdata('Ubu');
::ok( Ray->can('Ubu') );
::ok( Ray->can('_Ubu_accessor') );

package Gun;
use base qw(Ray);

::ok( Gun->can('Ubu') );
Gun->Ubu('Pere');
::ok( Gun->Ubu eq 'Pere');

package Suitcase;
use base qw(Gun);

# Test that superclasses effect children.
::ok( Suitcase->can('Ubu') );
::ok( Suitcase->Ubu('Pere'));
::ok( Suitcase->can('_Ubu_accessor') );

# Test that superclasses don't effect overriden children.
Ray->Ubu('Squonk');
::ok( Ray->Ubu eq 'Squonk');
::ok( Gun->Ubu eq 'Pere');
::ok( Suitcase->Ubu eq 'Pere');
