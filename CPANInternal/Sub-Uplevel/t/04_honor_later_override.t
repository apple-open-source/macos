#!/usr/bin/perl -Tw

use lib qw(t/lib);
use strict;
use Test::More tests => 7;

# Goal of these tests: confirm that Sub::Uplevel will honor (use) a
# CORE::GLOBAL::caller that occurs after Sub::Uplevel is loaded

#--------------------------------------------------------------------------#
# define a custom caller function that reverses the package name
#--------------------------------------------------------------------------#

sub _reverse_caller(;$) { 
    my $height = $_[0];
    my @caller = CORE::caller(++$height);
    $caller[0] = defined $caller[0] ? reverse $caller[0] : undef;
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
# load Sub::Uplevel then redefine CORE::GLOBAL::caller
#--------------------------------------------------------------------------#

BEGIN {
    ok( ! defined *CORE::GLOBAL::caller{CODE}, 
        "no global override yet" 
    );

    use_ok('Sub::Uplevel');

    is( *CORE::GLOBAL::caller{CODE}, \&Sub::Uplevel::_normal_caller,
        "Sub::Uplevel's normal caller override in place"
    );

    # old style no warnings 'redefine'
    my $old_W = $^W;
    $^W = 0;
        
    *CORE::GLOBAL::caller = \&_reverse_caller;
    $^W = $old_W

}

is( *CORE::GLOBAL::caller{CODE}, \&_reverse_caller, 
    "added new, custom caller override"
);

#--------------------------------------------------------------------------#
# define subs *after* caller has been redefined in BEGIN
#--------------------------------------------------------------------------#

sub test_caller { return scalar caller }

sub uplevel_caller { return uplevel 1, \&test_caller }

sub test_caller_w_uplevel { return uplevel_caller }

#--------------------------------------------------------------------------#
# Test for reversed package name both inside and outside an uplevel call
#--------------------------------------------------------------------------#

is( scalar caller(), undef,
    "caller from main package is undef"
);

is( test_caller(), reverse("main"),
    "caller from subroutine calls custom routine"
);

is( test_caller_w_uplevel(), reverse("main"),
    "caller from uplevel subroutine calls custom routine"
);

