# $Id: 1basic.t 1039 2003-05-30 14:04:49Z cfaerber $
use Test::More tests => 3;
BEGIN { 
  use_ok('DateTime::Format::Pg')
};

{
  my $dt = DateTime::Format::Pg->parse_datetime('2003-01-01 19:00:00.123+09:30');
  isa_ok($dt,'DateTime');
}

{
  eval {
    my $dt = DateTime::Format::Pg->parse_datetime('THIS DATE IS INVALID');
    fail();
  } || pass();
}
