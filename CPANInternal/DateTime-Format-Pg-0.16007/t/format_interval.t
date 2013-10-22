# $Id: format_interval.t 3403 2006-03-31 22:04:06Z lestrrat $
use Test::More tests => 4;
use DateTime 0.10;
use DateTime::Duration;
use DateTime::Format::Pg 0.02;

%tests = (
  '@ 43 months 1 days' => {
    years      => 3,
    months     => 7,
    days       => 1, },

  '@ 210 days' => {
    weeks     => 30, },
    
  '@ 121 minutes 61 seconds' => {
    hours     => 1,
    minutes   => 61,
    seconds   => 61, },

  '@ 1 months 0.000003 seconds' => {
    months      => 1,
    nanoseconds => 3000, },
);

foreach my $result (keys %tests) {
  my $dt = DateTime::Duration->new( %{$tests{$result}} );
  is( DateTime::Format::Pg->format_interval($dt), $result );
}
