# pod.t
#
# Test suite for File::Path - test the POD
#
# copyright (C) 2007 David Landgren

use strict;
use Test::More;

if (!$ENV{PERL_AUTHOR_TESTING}) {
    plan skip_all => 'PERL_AUTHOR_TESTING environment variable not set (or zero)';
    exit;
}

my @file;
if (open MAN, '< MANIFEST') {
    while (<MAN>) {
        chomp;
        push @file, $_ if /\.pm$/;
    }
    close MAN;
}
else {
    diag "failed to read MANIFEST: $!";
}

my @coverage = qw(
    File::Path
);

my $test_pod_tests = eval "use Test::Pod"
    ? 0 : @file;

my $test_pod_coverage_tests = eval "use Test::Pod::Coverage"
    ? 0 : @coverage;

if ($test_pod_tests + $test_pod_coverage_tests) {
    plan tests => @file + @coverage;
}
else {
    plan skip_all => 'POD testing modules not installed';
}

SKIP: {
    skip( 'Test::Pod not installed on this system', scalar(@file) )
        unless $test_pod_tests;
    pod_file_ok($_) for @file;
}

SKIP: {
    skip( 'Test::Pod::Coverage not installed on this system', scalar(@coverage) )
        unless $test_pod_coverage_tests;
    pod_coverage_ok( $_, "$_ POD coverage is go!" ) for @coverage;
}
