BEGIN { $| = 1; print "1..2\n"; }

use JSON::XS;
use Tie::Hash;
use Tie::Array;

our $test;
sub ok($;$) {
   print $_[0] ? "" : "not ", "ok ", ++$test, "\n";
}

my $js = JSON::XS->new;

tie my %h, 'Tie::StdHash';
%h = (a => 1);

ok ($js->encode (\%h) eq '{"a":1}');

tie my @a, 'Tie::StdArray';
@a = (1, 2);

ok ($js->encode (\@a) eq '[1,2]');
