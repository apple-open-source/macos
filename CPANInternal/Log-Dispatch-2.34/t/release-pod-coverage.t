
BEGIN {
  unless ($ENV{RELEASE_TESTING}) {
    require Test::More;
    Test::More::plan(skip_all => 'these tests are for release candidate testing');
  }
}

use strict;
use warnings;

use Test::More;

eval "use Test::Pod::Coverage 1.04";
plan skip_all => "Test::Pod::Coverage 1.04 required for testing POD coverage"
    if $@;

all_pod_coverage_ok(
    {
        trustme => [
            qr/^(?:warn|err|crit|emerg)$/,
            qr/^send_email|log_message|new$/,
            qr/^add_callback$/,
            qr/^(?:O_)?APPEND$/,
            qr/^is_\w+$/
        ]
    }
);
