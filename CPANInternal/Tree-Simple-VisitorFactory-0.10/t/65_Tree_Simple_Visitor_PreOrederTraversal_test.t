#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 8;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::PreOrderTraversal');
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

can_ok("Tree::Simple::Visitor::PreOrderTraversal", 'new');

my $visitor = Tree::Simple::Visitor::PreOrderTraversal->new();
isa_ok($visitor, 'Tree::Simple::Visitor::PreOrderTraversal');
isa_ok($visitor, 'Tree::Simple::Visitor');

can_ok($visitor, 'visit');
can_ok($visitor, 'getResults');

$tree->accept($visitor);
is_deeply(
    [ $visitor->getResults() ],
    [ qw(1 1.1 1.2 1.2.1 1.2.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1) ],
    '... our results are as expected');
    