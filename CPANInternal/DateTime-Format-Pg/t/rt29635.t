use strict;
use Test::More (tests => 4);

BEGIN
{
    use_ok("DateTime::Format::Pg");
}

my $dt = DateTime::Format::Pg->parse_time('16:05:00');

is($dt->hour, 16);
is($dt->minute, 5);
is($dt->second, 0);

