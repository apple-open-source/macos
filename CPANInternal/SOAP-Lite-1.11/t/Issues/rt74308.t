use strict;
use warnings;
use Test::More tests => 1;

use SOAP::Lite;
eval {
    my $soap = SOAP::Lite->service("file:$0.wsdl");
};

ok (! $@);
