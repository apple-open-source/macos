# $Id: parse_time.t 1055 2005-03-16 16:13:19Z cfaerber $
use Test::More tests => 10;
use DateTime::Format::Pg 0.08;

{
  my $dt = DateTime::Format::Pg->parse_time('19:18:17.123+09:30');
  is($dt->hour(),              19, 'hour');
  is($dt->minute(),            18, 'minute');
  is($dt->second(),            17, 'second');
  is($dt->nanosecond(), 123000000, 'nanosecond');
  is($dt->offset(),  		0, 'tz offset');
}

{
  my $dt = DateTime::Format::Pg->parse_timetz('19:18:17.123+09:30');
  is($dt->hour(),              19, 'hour');
  is($dt->minute(),            18, 'minute');
  is($dt->second(),            17, 'second');
  is($dt->nanosecond(), 123000000, 'nanosecond');
  is($dt->offset(),  (9*60+30)*60, 'tz offset');
}
