#============================================================= -*-perl-*-
#
# t/pod_coverage.t
#
# Use Test::Pod::Coverage (if available) to test the POD documentation.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 2008-2012 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib );
use Test::More;

plan( skip_all => "Author tests not required for installation" )
    unless $ENV{ RELEASE_TESTING   }
        or $ENV{ AUTOMATED_TESTING };

eval "use Test::Pod::Coverage 1.00";
plan skip_all => "Test::Pod::Coverage 1.00 required for testing POD coverage" if $@;
plan tests => 11;

# still got some work to do on getting all modules full documented
pod_coverage_ok('Template');
pod_coverage_ok('Template::Base');
pod_coverage_ok('Template::Config');
pod_coverage_ok('Template::Context');
pod_coverage_ok('Template::Document');
#pod_coverage_ok('Template::Exception');
#pod_coverage_ok('Template::Filters');
pod_coverage_ok('Template::Iterator');
#pod_coverage_ok('Template::Parser');
#pod_coverage_ok('Template::Plugin');
pod_coverage_ok('Template::Plugins');
pod_coverage_ok('Template::Provider');
pod_coverage_ok('Template::Service');
pod_coverage_ok('Template::Stash');
#pod_coverage_ok('Template::Test');
#pod_coverage_ok('Template::View');
#pod_coverage_ok('Template::VMethods');
pod_coverage_ok('Template::Namespace::Constants');
#pod_coverage_ok('Template::Stash::Context');
#pod_coverage_ok('Template::Stash::XS');

#all_pod_coverage_ok();
