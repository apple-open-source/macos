# $Id: format_date.t,v 1.2 2003/05/30 14:04:49 cfaerber Exp $
use Test::More tests => 3;
use DateTime 0.10;
use DateTime::Format::Pg 0.02;

%tests = (
  '2003-07-01' => {
    year      => 2003,
    month     => 7,
    day	      => 1, },

  '1900-01-01' => {
    year      => 1900,
    month     => 1,
    day	      => 1, },
    
  '0001-12-24 BC' => {
    year      => 0,
    month     => 12,
    day	      => 24, },
);

foreach my $result (keys %tests) {
  my $dt = DateTime->new( %{$tests{$result}} );
  is( DateTime::Format::Pg->format_date($dt), $result );
}
