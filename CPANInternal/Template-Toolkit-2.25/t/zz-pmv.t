#!/usr/bin/perl

# Test that our declared minimum Perl version matches our syntax

use strict;
BEGIN {
    $|  = 1;
    $^W = 1;
}

my @MODULES = (
    'Perl::MinimumVersion 1.20',
    'Test::MinimumVersion 0.008',
);

# Don't run tests for installs
use Test::More;

# NOTE: Perl::MinimumVersion / PPI can't parse hash definitions with utf8 
# values or keys.  That means that t/stash-xs-unicode.t always fails.  We
# have no option but to disable this test until PPI can handle this case
# or Test::MinimumVersion gives us a way to specify files to skip.

plan( skip_all => "These aren't the tests you're looking for... move along" );

# NOTHING RUN PAST THIS POINT

unless ( $ENV{AUTOMATED_TESTING} or $ENV{RELEASE_TESTING} ) {
    plan( skip_all => "Author tests not required for installation" );
}

# Load the testing modules
foreach my $MODULE ( @MODULES ) {
    eval "use $MODULE";
    if ( $@ ) {
        $ENV{RELEASE_TESTING}
            ? die( "Failed to load required release-testing module $MODULE" )
            : plan( skip_all => "$MODULE not available for testing" );
    }
}

#all_minimum_version_ok(5.006);
minimum_version_ok('t/stash-xs-unicode.t', 5.006);
