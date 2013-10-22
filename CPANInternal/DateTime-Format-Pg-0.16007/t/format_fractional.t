use strict;
use warnings;

use Test::More tests => 1;

use DateTime;
use DateTime::Format::Pg;

my $dt1 = DateTime->new(
   year       => 2010,
   month      => 6,
   day        => 21,
   hour       => 10,
   minute     => 11,
   second     => 9,
   nanosecond => 66727999,
   time_zone  => 'Australia/Brisbane',

);

my $ts1 = DateTime::Format::Pg->format_datetime($dt1);

my $dt2 = DateTime::Format::Pg->parse_datetime($ts1);
my $ts2 = DateTime::Format::Pg->format_datetime($dt2);

is($ts1, $ts2, 'fractional timestamptz was parsed/formatted correctly');
