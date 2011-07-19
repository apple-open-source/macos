use strict;
use warnings;

use Test::More;

eval "use Test::Output";
if ($@) {
    plan skip_all => 'These tests require Test::Output.';
}

plan tests => 2;

use DateTime::Locale;

my $loc = DateTime::Locale->load('en');

my @months;
my $sub = sub {
    for my $m ( 1 .. 12 ) {
        my $dt = bless { m => $m }, 'FakeDateTime';
        push @months, $loc->month_abbreviation($dt);
    }
};

stderr_like(
    $sub,
    qr/month_abbreviation method in DateTime::Locale::Base has been deprecated/,
    'got a deprecation warning for month_abbreviation'
);
is_deeply(
    \@months, [qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec )],
    'month_abbreviation returns the right data'
);

{

    package FakeDateTime;

    sub month_0 { $_[0]->{m} - 1 }
}
