#!perl -T

use Test::More;
eval "use Test::Pod::Coverage 1.04";
plan skip_all => "Test::Pod::Coverage 1.04 required for testing POD coverage" if $@;
my $params = {
    trustme => [qr/^ERROR$/]
};
all_pod_coverage_ok($params);
