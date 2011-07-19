#! /usr/bin/perl

use strict;
use warnings;
use Test::More skip_all => 'stuff relating to RT#24678 that I have not fixed yet';

use Test::Exception tests => 12;

sub A1::DESTROY {eval{}}
dies_ok { my $x = bless [], 'A1'; die } q[Unlocalized $@ for eval{} during DESTROY];

sub A2::DESTROY {die 43 }
throws_ok { my $x = bless [], 'A2'; die 42} qr/42.+43/s, q[Died with the primary and secondar errors];

sub A2a::DESTROY { die 42 }
throws_ok { my $obj = bless [], 'A2a'; die 43 } qr/43/, 
    q[Of multiple failures, the "primary" one is returned];

{
    sub A3::DESTROY {die}
    dies_ok { my $x = bless [], 'A3'; 1 } q[Death during destruction for success is noticed];
}


sub A4::DESTROY {delete$SIG{__DIE__};eval{}}
dies_ok { my $x = bless [], 'A4'; die } q[Unlocalized $@ for eval{} during DESTROY];

sub A5::DESTROY {delete$SIG{__DIE__};die 43 }
throws_ok { my $x = bless [], 'A5'; die 42} qr/42.+43/s, q[Died with the primary and secondar errors];

TODO: {
    our $TODO = q[No clue how to solve this one.];
    sub A6::DESTROY {delete$SIG{__DIE__};die}
    dies_ok { my $x = bless [], 'A6'; 1 } q[Death during destruction for success is noticed];
}


dies_ok { die bless [], 0 } q[Died with a "false" exception class];
dies_ok { die bless [], "\0" } q[Died with a "false" exception class];

package A7;
use overload bool => sub { 0 }, '0+' => sub { 0 }, '""' => sub { '' }, fallback => 1;
package main;
dies_ok { die bless [], 'A7' } q[False overloaded exceptions are noticed];


$main::{'0::'} = $main::{'A7::'};
dies_ok { die bless [], 0 } q[Died a false death];


package A8;
use overload bool => sub {eval{};0}, '0+' => sub{eval{};0}, '""' => sub { eval{}; '' }, fallback => 1;
package main;
dies_ok { die bless [], 'A8' } q[Evanescent exceptions are noticed];


__END__

 dies_ok{ my $foo = Foo->new; die "Fatal Error" };
 lives_ok{ my $foo = Foo->new; die "Fatal Error" };

 not ok 1
 # Code died, but appeared to live because $@ was reset
 # unexpectedly by a DESTROY method called during cleanup
 not ok 2
 # Code died, but appeared to live because $@ was reset
 # unexpectedly by a DESTROY method called during cleanup
