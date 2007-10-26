#!/usr/bin/perl

use strict;
use Test;
BEGIN { plan tests => 2 }

use Module::Build;
use File::Spec;

my $file = File::Spec->catfile('t', 'Sample', 'lib', 'Sample.pm');
ok( Module::Build->version_from_file( $file ), '0.01', 'version_from_file' );

ok( Module::Build->compare_versions( '1.01_01', '>', '1.01' ), 1, 'compare: 1.0_01 > 1.0' );
