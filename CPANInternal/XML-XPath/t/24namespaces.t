use Test;
BEGIN { plan tests => 9 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;

# Don't set namespace prefixes - uses element context namespaces

@nodes = $xp->findnodes('//foo:foo'); # should find foobar.com foos
ok(@nodes, 3);

@nodes = $xp->findnodes('//goo:foo'); # should find no foos
ok(@nodes, 0);

@nodes = $xp->findnodes('//foo'); # should find default NS foos
ok(@nodes, 2);

# Set namespace mappings.

$xp->set_namespace("foo" => "flubber.example.com");
$xp->set_namespace("goo" => "foobar.example.com");

# warn "TEST 6\n";
@nodes = $xp->findnodes('//foo:foo'); # should find flubber.com foos
# warn "found: ", scalar @nodes, "\n";
ok(@nodes, 2);

@nodes = $xp->findnodes('//goo:foo'); # should find foobar.com foos
ok(@nodes, 3);

@nodes = $xp->findnodes('//foo'); # should find default NS foos
ok(@nodes, 2);

ok($xp->findvalue('//attr:node/@attr:findme'), 'someval');

__DATA__
<xml xmlns:foo="foobar.example.com"
    xmlns="flubber.example.com">
    <foo>
        <bar/>
        <foo/>
    </foo>
    <foo:foo>
        <foo:foo/>
        <foo:bar/>
        <foo:bar/>
        <foo:foo/>
    </foo:foo>
    <attr:node xmlns:attr="attribute.example.com"
        attr:findme="someval"/>
</xml>
