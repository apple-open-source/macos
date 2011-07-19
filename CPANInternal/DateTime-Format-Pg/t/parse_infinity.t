# $Id: parse_infinity.t 1061 2006-01-07 00:45:49Z lestrrat $
use Test::More tests => 4;
use DateTime::Format::Pg 0.02;

{
  my $dt = DateTime::Format::Pg->parse_datetime('infinity');
  isa_ok($dt, 'DateTime::Infinite::Future');
}

{
  my $dt = DateTime::Format::Pg->parse_timestamp('infinity');
  isa_ok($dt, 'DateTime::Infinite::Future');
}

{
  my $dt = DateTime::Format::Pg->parse_datetime('-infinity');
  isa_ok($dt, 'DateTime::Infinite::Past');
}

{
  my $dt = DateTime::Format::Pg->parse_timestamp('-infinity');
  isa_ok($dt, 'DateTime::Infinite::Past');
}
