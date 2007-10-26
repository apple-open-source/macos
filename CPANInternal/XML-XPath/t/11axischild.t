use Test;
BEGIN { plan tests => 6 }

use XML::XPath::Parser;

ok(1);

my $xp = XML::XPath::Parser->new();
ok($xp);

ok($xp->parse('/AAA')->as_string, "(/child::AAA)");

ok($xp->parse('/AAA/BBB')->as_string, "(/child::AAA/child::BBB)");

ok($xp->parse('/child::AAA/child::BBB')->as_string,
        "(/child::AAA/child::BBB)");

ok($xp->parse('/child::AAA/BBB')->as_string, "(/child::AAA/child::BBB)");
