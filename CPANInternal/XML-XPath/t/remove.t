#!/usr/bin/perl

use Test;
BEGIN { plan tests => 7 }

use XML::XPath;
use XML::XPath::XMLParser;
$XML::XPath::SafeMode = 1;

ok(1);
my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my ($root) = $xp->findnodes('/');

ok($root);

($root) = $root->getChildNodes;
my @nodes = $xp->findnodes('//Cart',$root);

ok(@nodes, 2);

$root->removeChild($nodes[0]);

@nodes = $xp->findnodes('//Cart', $root);
ok(@nodes, 1);

my $cart = $nodes[0];

@nodes = $xp->findnodes('//Cart/@*', $root);
ok(@nodes, 2);

$cart->removeAttribute('crap');
@nodes = $xp->findnodes('//Cart/@*', $root);

ok(@nodes, 1);

__DATA__
<Shop id="mod3838" hello="you">
<Cart id="1" crap="crap">
        <Item id="11" crap="crap"/>
</Cart>
<Cart id="2" crap="crap"/>
</Shop>
