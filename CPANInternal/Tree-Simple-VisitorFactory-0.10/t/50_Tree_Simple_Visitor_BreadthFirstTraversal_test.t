#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 15;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::BreadthFirstTraversal');
}

use Tree::Simple;

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.1"),
                                            Tree::Simple->new("1.2")
                                                        ->addChildren(
                                                            Tree::Simple->new("1.2.1"),
                                                            Tree::Simple->new("1.2.2")
                                                        ),
                                            Tree::Simple->new("1.3")                                                                                                
                                        ),
                            Tree::Simple->new("2")
                                        ->addChildren(
                                            Tree::Simple->new("2.1"),
                                            Tree::Simple->new("2.2")
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

can_ok("Tree::Simple::Visitor::BreadthFirstTraversal", 'new');

my $visitor = Tree::Simple::Visitor::BreadthFirstTraversal->new();
isa_ok($visitor, 'Tree::Simple::Visitor::BreadthFirstTraversal');
isa_ok($visitor, 'Tree::Simple::Visitor');

can_ok($visitor, 'visit');
can_ok($visitor, 'getResults');

$tree->accept($visitor);
is_deeply(
    [ $visitor->getResults() ],
    [ qw(1 2 3 4 1.1 1.2 1.3 2.1 2.2 3.1 3.2 3.3 4.1 1.2.1 1.2.2) ],
    '... our results are as expected');

can_ok($visitor, 'setNodeFilter');
$visitor->setNodeFilter(sub { "Tree_" . $_[0]->getNodeValue() });

can_ok($visitor, 'includeTrunk');
$visitor->includeTrunk(1);

$tree->accept($visitor);
is_deeply(
    [ $visitor->getResults() ],
    [ qw(Tree_root Tree_1 Tree_2 Tree_3 Tree_4 Tree_1.1 
         Tree_1.2 Tree_1.3 Tree_2.1 Tree_2.2 Tree_3.1 
         Tree_3.2 Tree_3.3 Tree_4.1 Tree_1.2.1 Tree_1.2.2) ],
    '... our results are as expected');
    
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
