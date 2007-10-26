# $Id: format_interval.t,v 1.1 2005/03/16 16:50:40 cfaerber Exp $
use Test::More tests => 3;
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
);

foreach my $result (keys %tests) {
  my $dt = DateTime::Duration->new( %{$tests{$result}} );
  is( DateTime::Format::Pg->format_interval($dt), $result );
}
