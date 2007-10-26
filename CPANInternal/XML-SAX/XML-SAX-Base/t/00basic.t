use Test;
BEGIN { plan tests => 1 }
END { ok($loaded) }
use XML::SAX::Base;
$loaded++;

