# $Id: parse_date.t 1039 2003-05-30 14:04:49Z cfaerber $
use Test::More tests => 18;
use DateTime 0.10;
use DateTime::TimeZone 0.12;
use DateTime::Format::Pg 0.02;

my $dt;

# ISO 
#
$dt = DateTime::Format::Pg->parse_date('2003-04-26');
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('1900-01-01');
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('0001-12-24 BC');
is($dt->ymd(), '0000-12-24');

# PostgreSQL
#
$dt = DateTime::Format::Pg->parse_date('26-04-2003', 'european' => 1);
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('01-01-1900', 'european' => 1);
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('24-12-0001 BC', 'european' => 1);
is($dt->ymd(), '0000-12-24');

$dt = DateTime::Format::Pg->parse_date('04-26-2003', 'european' => 0);
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('01-01-1900', 'european' => 0);
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('12-24-0001 BC', 'european' => 0);
is($dt->ymd(), '0000-12-24');

# SQL
#
$dt = DateTime::Format::Pg->parse_date('26/04/2003', 'european' => 1);
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('01/01/1900', 'european' => 1);
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('24/12/0001 BC', 'european' => 1);
is($dt->ymd(), '0000-12-24');

$dt = DateTime::Format::Pg->parse_date('04/26/2003', 'european' => 0);
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('01/01/1900', 'european' => 0);
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('12/24/0001 BC', 'european' => 0);
is($dt->ymd(), '0000-12-24');

# German
#
$dt = DateTime::Format::Pg->parse_date('26.04.2003');
is($dt->ymd(), '2003-04-26');

$dt = DateTime::Format::Pg->parse_date('01.01.1900');
is($dt->ymd(), '1900-01-01');

$dt = DateTime::Format::Pg->parse_date('24.12.0001 BC');
is($dt->ymd(), '0000-12-24');
