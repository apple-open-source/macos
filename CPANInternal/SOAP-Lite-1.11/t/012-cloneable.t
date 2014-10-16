use strict;
use warnings;
use Test;

use SOAP::Lite;

plan tests => 2;

my $soap = SOAP::Lite->new(
    readable => 1,
    outputxml => 1,
);

my $clone = $soap->clone();

ok ($clone->readable() == 1);
ok ($clone->outputxml() == 1);
