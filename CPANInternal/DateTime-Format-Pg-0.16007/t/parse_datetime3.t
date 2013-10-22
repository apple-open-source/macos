# $Id: $
use Test::More tests => 8;
use DateTime::Format::Pg 0.02;

{
  my $dt = DateTime::Format::Pg->parse_datetime('1221-01-02 03:04:05+01 BC');
  is($dt->year(),            -1220, 'year');
  is($dt->month(),             01, 'month');
  is($dt->day(),               02, 'day');
  is($dt->hour(),              03, 'hour');
  is($dt->minute(),            04, 'minute');
  is($dt->second(),            05, 'second');
  is($dt->nanosecond(),        00, 'nanosecond');
  is($dt->offset(),  (1*60)*60, 'tz offset');
}
