use strict;
use Test::More (tests => 1);

use DateTime::Duration;
use DateTime::Format::Pg;

my $dur = DateTime::Duration->new(nanoseconds => 3000);
my $val = DateTime::Format::Pg->format_duration($dur);

is($val, '@ 0.000003 seconds');