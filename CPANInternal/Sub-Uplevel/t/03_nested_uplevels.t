#!perl
use strict;
use Test::More;

use Sub::Uplevel;

package Wrap;
use Sub::Uplevel;

sub wrap {
    my ($n, $f, $depth, $up, @case) = @_;
    
    if ($n > 1) {
        $n--;
        return wrap( $n, $f, $depth, $up, @case );
    }
    else {
        return uplevel( $up , $f, $depth, $up, @case );
    }
}

package Call;

sub recurse_call_check {
    my ($depth, $up, @case) = @_;

    if ( $depth ) {
        $depth--;
        my @result;
        push @result, recurse_call_check($depth, $up, @case, 'Call' );
        for my $n ( 1 .. $up ) {
            push @result, Wrap::wrap( $n, \&recurse_call_check, 
                $depth, $n, @case, 
                $n == 1 ? "Wrap(Call)" : "Wrap(Call) x $n" ),
            ;
        }
        return @result;
    }
    else {
        my (@uplevel_callstack, @real_callstack);
        my $i = 0;
        while ( defined( my $caller = caller($i++) ) ) {
            push @uplevel_callstack, $caller;
        }
        $i = 0;
        while ( defined( my $caller = CORE::caller($i++) ) ) {
            push @real_callstack, $caller;
        }
        return [ 
            join( q{, }, @case ),
            join( q{, }, reverse @uplevel_callstack ),
            join( q{, }, reverse @real_callstack ),
        ];      
    }
}

package main;

my $depth = 4;
my $up = 3;
my $cases = 104;

plan tests => $cases;

my @results = Call::recurse_call_check( $depth, $up, 'Call' );

is( scalar @results, $cases, 
    "Right number of cases"
);

my $expected = shift @results;

for my $got ( @results ) {
    is( $got->[1], $expected->[1], 
        "Case: $got->[0]"
    ) or diag( "Real callers: $got->[2]" );
}

