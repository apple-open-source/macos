#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 23;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::PathToRoot');
}

use Tree::Simple;

my $very_deep = Tree::Simple->new("1.2.2.1");
isa_ok($very_deep, 'Tree::Simple');

my $kind_of_deep = Tree::Simple->new("2.2.1");
isa_ok($kind_of_deep, 'Tree::Simple');

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.1"),
                                            Tree::Simple->new("1.2")
                                                        ->addChildren(
                                                            Tree::Simple->new("1.2.1"),
                                                            Tree::Simple->new("1.2.2")
                                                                        ->addChild($very_deep)
                                                        ),
                                            Tree::Simple->new("1.3")                                                                                                
                                        ),
                            Tree::Simple->new("2")
                                        ->addChildren(
                                            Tree::Simple->new("2.1"),
                                            Tree::Simple->new("2.2")
                                                        ->addChild($kind_of_deep)
                                        ),                            
                            Tree::Simple->new("3")
                                        ->addChildren(
                                            Tree::Simple->new("3.1"),
                                            Tree::Simple->new("3.2"),
                                            Tree::Simple->new("3.3")                                                                                                
                                        ),                            
                            Tree::Simple->new("4")                                                        
                                        ->addChildren(
                                            Tree::Simple->new("4.1")
                                        )                            
                       );
isa_ok($tree, 'Tree::Simple');

can_ok("Tree::Simple::Visitor::PathToRoot", 'new');

my $visitor = Tree::Simple::Visitor::PathToRoot->new();
isa_ok($visitor, 'Tree::Simple::Visitor::PathToRoot');
isa_ok($visitor, 'Tree::Simple::Visitor');

can_ok($visitor, 'visit');
can_ok($visitor, 'getPathAsString');
can_ok($visitor, 'getPath');

$kind_of_deep->accept($visitor);

is($visitor->getPathAsString("/"), "2/2.2/2.2.1", '... our paths match');
is_deeply(
    [ $visitor->getPath() ], 
    [ qw/2 2.2 2.2.1/ ], 
    '... our paths match');

can_ok($visitor, 'setNodeFilter');
$visitor->setNodeFilter(sub { "~" . $_[0]->getNodeValue() . "~" });

$visitor->includeTrunk(1);

$very_deep->accept($visitor);

is($visitor->getPathAsString(), "~root~, ~1~, ~1.2~, ~1.2.2~, ~1.2.2.1~", '... our paths match again');
is_deeply(
    [ $visitor->getPath() ], 
    [ qw/~root~ ~1~ ~1.2~ ~1.2.2~ ~1.2.2.1~/ ], 
    '... our paths match again');

$visitor->includeTrunk(0);

$tree->accept($visitor);
is($visitor->getPathAsString("|"), "", '... we got nothing');
is_deeply(
    scalar $visitor->getPath(), 
    [ ], 
    '... no path means no results');

$visitor->includeTrunk(1);

$tree->accept($visitor);
is($visitor->getPathAsString(), "~root~", '... we got nothing');
is_deeply(
    scalar $visitor->getPath(), 
    [ "~root~" ], 
    '... but include root and we have something at least');

# test some error conditions

throws_ok {
    $visitor->visit();
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->visit("Fail");
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->visit([]);
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->visit(bless({}, "Fail"));
} qr/Insufficient Arguments/, '... this should die';
