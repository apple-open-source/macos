#!/usr/bin/perl

use Test;
BEGIN { plan tests => 8 }

use XML::XPath;
use XML::XPath::Node::Comment;
#$XML::XPath::SafeMode = 1;

ok(1);
my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my ($root) = $xp->findnodes('/');

ok($root);

($root) = $root->getChildNodes;
my @nodes = $root->findnodes('//Cart');

ok(@nodes, 2);

my $comment = XML::XPath::Node::Comment->new("Before Comment");

$root->insertBefore($comment, $nodes[0]);

my $other_comment = XML::XPath::Node::Comment->new("After Comment");

$root->insertAfter($other_comment, $nodes[0]);

@nodes = $xp->findnodes('/Shop/node()');

# foreach (@nodes) {
#     print STDERR $_->toString;
# }

ok($nodes[1]->isCommentNode);
ok($nodes[3]->isCommentNode);

my ($before) = $xp->findnodes('/Shop/comment()[contains( string() , "Before")]');
ok($before->get_pos, 1);

my ($after) = $xp->findnodes('/Shop/comment()[contains( string() , "After")]');
ok($after->get_pos, 3);


__DATA__
<Shop id="mod3838" hello="you">
<Cart id="1" crap="crap">
        <Item id="11" crap="crap"/>
</Cart>
<Cart id="2" crap="crap"/>
</Shop>
