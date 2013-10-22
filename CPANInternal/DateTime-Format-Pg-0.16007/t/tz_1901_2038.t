# $Id: tz_1901_2038.t 1039 2003-05-30 14:04:49Z cfaerber $
use Test::More tests => 4;
use DateTime 0.10;
use DateTime::TimeZone 0.12;
use DateTime::Format::Pg 0.02;

# 2038-01-18 23:59:59+00
# 2038-01-19 00:00:00
# 1901-12-14 00:00:00+00
# 1901-12-13 23:59:59

my $dt;

$dt = DateTime::Format::Pg->parse_timestamptz('1901-12-13 23:59:59');
ok($dt->time_zone->is_utc);

$dt = DateTime::Format::Pg->parse_timestamptz('2038-01-19 00:00:00');
ok($dt->time_zone->is_utc);

$dt = DateTime::Format::Pg->parse_timestamptz('1901-12-14 00:00:00');
ok($dt->time_zone->is_floating);

$dt = DateTime::Format::Pg->parse_timestamptz('2038-01-18 23:59:00');
ok($dt->time_zone->is_floating);
