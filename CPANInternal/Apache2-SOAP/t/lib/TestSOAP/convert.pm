package TestSOAP::convert;
use strict;
use warnings;

sub convert {
  my ($self, %args) = @_;
  my $mode = $args{mode};
  my $string = $args{string};
  my $response = ($mode eq 'uc') ? uc($string) : lc($string);
  return {results => $response};
}

1;
