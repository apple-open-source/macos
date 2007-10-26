#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 22;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::GetAllDescendents');
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

can_ok("Tree::Simple::Visitor::GetAllDescendents", 'new');

my $visitor = Tree::Simple::Visitor::GetAllDescendents->new();
isa_ok($visitor, 'Tree::Simple::Visitor::GetAllDescendents');
isa_ok($visitor, 'Tree::Simple::Visitor');

can_ok($visitor, 'visit');
can_ok($visitor, 'getAllDescendents');

can_ok($visitor, 'setTraversalMethod');
can_ok($visitor, 'setNodeFilter');

$tree->accept($visitor);

is_deeply(
    [ $visitor->getAllDescendents() ], 
    [ qw/1 1.1 1.2 1.2.1 1.2.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1/ ], 
    '... our descendents match');

can_ok($visitor, 'setNodeFilter');
$visitor->setNodeFilter(sub { "*" . $_[0]->getNodeValue() });

$tree->accept($visitor);

is_deeply(
    [ $visitor->getAllDescendents() ], 
    [ qw/*1 *1.1 *1.2 *1.2.1 *1.2.2 *1.3 *2 *2.1 *2.2 *3 *3.1 *3.2 *3.3 *4 *4.1/ ], 
    '... our paths descendents again');
    
use_ok('Tree::Simple::Visitor::BreadthFirstTraversal');    

$visitor->setNodeFilter(sub { $_[0]->getNodeValue() });
$visitor->setTraversalMethod(Tree::Simple::Visitor::BreadthFirstTraversal->new());
$tree->accept($visitor);

is_deeply(
    [ $visitor->getAllDescendents() ], 
    [ qw/1 2 3 4 1.1 1.2 1.3 2.1 2.2 3.1 3.2 3.3 4.1 1.2.1 1.2.2/ ], 
    '... our bredth-first descendents match');  
    
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

throws_ok {
    $visitor->setTraversalMethod();
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->setTraversalMethod("Fail");
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->setTraversalMethod([]);
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->setTraversalMethod(bless({}, "Fail"));
} qr/Insufficient Arguments/, '... this should die';   
