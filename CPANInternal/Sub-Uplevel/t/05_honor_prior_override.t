#!/usr/bin/perl -Tw

use lib qw(t/lib);
use strict;
use Test::More tests => 10;

# Goal of these tests: confirm that Sub::Uplevel will honor (use) a
# CORE::GLOBAL::caller override that occurs prior to Sub::Uplevel loading

#--------------------------------------------------------------------------#
# define a custom caller function that increments a counter
#--------------------------------------------------------------------------#

my $caller_counter = 0;
sub _count_caller(;$) { 
    $caller_counter++;
    my $height = $_[0];
    my @caller = CORE::caller(++$height);
    if( wantarray and !@_ ) {
        return @caller[0..2];
    }
    elsif (wantarray) {
        return @caller;
    }
    else {
        return $caller[0];
    }
}

#--------------------------------------------------------------------------#
# redefine CORE::GLOBAL::caller then load Sub::Uplevel 
#--------------------------------------------------------------------------#

BEGIN {
    ok( ! defined *CORE::GLOBAL::caller{CODE}, 
        "no global override yet" 
    );

    {
        # old style no warnings 'redefine'
        my $old_W = $^W;
        $^W = 0;
        *CORE::GLOBAL::caller = \&_count_caller;
        $^W = $old_W;
    }

    is( *CORE::GLOBAL::caller{CODE}, \&_count_caller,
        "added custom caller override"
    );

    use_ok('Sub::Uplevel');

    is( *CORE::GLOBAL::caller{CODE}, \&_count_caller,
        "custom caller override still in place"
    );


}

#--------------------------------------------------------------------------#
# define subs *after* caller has been redefined in BEGIN
#--------------------------------------------------------------------------#

sub test_caller { return scalar caller }

sub uplevel_caller { return uplevel 1, \&test_caller }

sub test_caller_w_uplevel { return uplevel_caller }

#--------------------------------------------------------------------------#
# Test for reversed package name both inside and outside an uplevel call
#--------------------------------------------------------------------------#

my $old_caller_counter; 

$old_caller_counter = $caller_counter;
is( scalar caller(), undef,
    "caller from main package is undef"
);
ok( $caller_counter > $old_caller_counter, "custom caller() was used" );

$old_caller_counter = $caller_counter;
is( test_caller(), "main",
    "caller from subroutine is main"
);
ok( $caller_counter > $old_caller_counter, "custom caller() was used" );

$old_caller_counter = $caller_counter;
is( test_caller_w_uplevel(), "main",
    "caller from uplevel subroutine is main"
);
ok( $caller_counter > $old_caller_counter, "custom caller() was used" );

