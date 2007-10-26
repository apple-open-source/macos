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
use Class::WhiteHole;
$loaded = 1;
ok(1, 'compile');
######################### End of black magic.

# Utility testing functions.
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
BEGIN { $Total_tests = 6 }

package Moo;
sub AUTOLOAD { return "AUTOLOADER!" }

package Test;

sub foo { return 456 }
@Test::ISA = qw(Class::WhiteHole Moo);

::ok( Test->foo == 456,         "static methods work" );
::ok( !eval { Test->bar; 1; },  "autoloader blocked"  ); # must be line 57

# There's a precedence problem.  Can't pass this all at once.
my $ok = $@ eq qq{Can\'t locate object method "bar" via package "Test" at $0 line 57.\n};
::ok( $ok,                      "Dying message preserved");

::ok( Test->can('foo'),         "UNIVERSAL not effected" );

eval {
    my $test_obj = bless {}, 'Test';
};
::ok( !$@,                      "DESTROY() not effected" );
