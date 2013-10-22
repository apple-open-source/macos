# $Id: format_datetime.t 3645 2007-03-23 03:01:42Z lestrrat $
use Test::More tests => 11;
use DateTime 0.10;
use DateTime::TimeZone;
use DateTime::Format::Pg 0.02;

%tests = (
  '2003-07-01 04:05:06+0200' => {
    year      => 2003,
    month     => 7,
    day       => 1,
    hour      => 4,
    minute    => 5,
    second    => 6,
    time_zone => 'Europe/Berlin' },

  '2003-01-01 04:05:06+0100' => {
    year      => 2003,
    month     => 1,
    day       => 1,
    hour      => 4,
    minute    => 5,
    second    => 6,
    time_zone => 'Europe/Berlin' },

  '2003-07-01 04:05:06' => {
    year      => 2003,
    month     => 7,
    day       => 1,
    hour      => 4,
    minute    => 5,
    second    => 6,
    time_zone => 'floating' },

  '2003-07-01 04:05:06+0000' => {
    year      => 2003,
    month     => 7,
    day       => 1,
    hour      => 4,
    minute    => 5,
    second    => 6,
    time_zone => 'UTC' },

  '1901-01-01 02:00:00+0100' => {
    year      => 1901,
    month     => 1,
    day       => 1,
    hour      => 2,
    minute    => 0,
    second    => 0,
    time_zone => '+01:00' },

  '0001-12-24 02:00:00+0100 BC' => {
    year      => 0,
    month     => 12,
    day       => 24,
    hour      => 2,
    minute    => 0,
    second    => 0,
    time_zone => '+01:00' },

  '0001-12-24 02:00:00.000001234+0100 BC' => {
    year      => 0,
    month     => 12,
    day       => 24,
    hour      => 2,
    minute    => 0,
    nanosecond=> 1234,
    second    => 0,
    time_zone => '+01:00' },

);

foreach my $result (keys %tests) {
  my $dt = DateTime->new( %{$tests{$result}} );
  is( DateTime::Format::Pg->format_datetime($dt), $result );
}

is(
    DateTime::Format::Pg->format_datetime(DateTime::Infinite::Future->new),
    'infinity'
);

is(
    DateTime::Format::Pg->format_timestamp(DateTime::Infinite::Future->new),
    'infinity'
);

is(
    DateTime::Format::Pg->format_datetime(DateTime::Infinite::Past->new),
    '-infinity'
);

is(
    DateTime::Format::Pg->format_timestamp(DateTime::Infinite::Past->new),
    '-infinity'
);

