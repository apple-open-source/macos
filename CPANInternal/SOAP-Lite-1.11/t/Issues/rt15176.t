use strict;
use warnings;
use Test::More tests => 1;

use SOAP::Lite;

# When setting a value that contains wide characters (> U+00ff), it tries to encode as Base64 but then fails
eval {
   my $header = SOAP::Header->value("\N{U+00ff}");
};
ok (!@$);
