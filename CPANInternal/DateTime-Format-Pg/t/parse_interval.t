# $Id: parse_interval.t,v 1.2 2003/05/30 14:04:49 cfaerber Exp $
use Test::More tests => 8;
use DateTime::Format::Pg 0.02;

my @results = (
  { 'days' => -1 },
  { 'minutes' => -(23*60+59) },
  { 'days' => -1, 'minutes' => -1 },
  { 'months' => 1, 'days' => -1 },
);

my %tests = (
  '-1 days' => $results[0],
  '-23:59' => $results[1],
  '-1 days -00:01' => $results[2],
  '1 mon -1 days' => $results[3],

  '@ 1 day ago' => $results[0],
  '@ 23 hours 59 mins ago' => $results[1],
  '@ 1 day 1 min ago' => $results[2],
  '@ 1 mon -1 days' => $results[3],
);

foreach my $test (keys %tests) {
  my $du = DateTime::Format::Pg->parse_interval($test);
  is( duration_hash_to_string($du->deltas()),
      duration_hash_to_string(%{$tests{$test}}) );
}

# for better Test::More::is output
#
sub duration_hash_to_string {
  my %hash = @_;
  my @vals = ();
  foreach(qw (months days minutes seconds nanoseconds)) {
    push @vals, sprintf('%s=%d',$_,$hash{$_}) if $hash{$_};
  }
  return join(', ',@vals);
}
