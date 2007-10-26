use Test;
BEGIN { plan tests => 2}
END { ok(0) unless $loaded }
use XML::LibXML;
$loaded = 1;
ok(1);

my $p = XML::LibXML->new();
ok($p);

# warn "# $tstr2\n";
