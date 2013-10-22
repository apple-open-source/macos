#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 67;

BEGIN { 
	use_ok('Tree::Simple'); 
};


{ # test height (with pictures)
    
    my $tree = Tree::Simple->new();
    isa_ok($tree, 'Tree::Simple');
    
    my $D = Tree::Simple->new('D');
    isa_ok($D, 'Tree::Simple');
    
    $tree->addChild($D);
    
    #   |
    #  <D>
    
    cmp_ok($D->getHeight(), '==', 1, '... D has a height of 1');
    
    my $E = Tree::Simple->new('E');
    isa_ok($E, 'Tree::Simple');
    
    $D->addChild($E);
    
    #   |
    #  <D>
    #    \
    #    <E>
    
    cmp_ok($D->getHeight(), '==', 2, '... D has a height of 2');
    cmp_ok($E->getHeight(), '==', 1, '... E has a height of 1');
    
    my $F = Tree::Simple->new('F');
    isa_ok($F, 'Tree::Simple');
    
    $E->addChild($F);
    
    #   |
    #  <D>
    #    \
    #    <E>
    #      \
    #      <F>
    
    cmp_ok($D->getHeight(), '==', 3, '... D has a height of 3');
    cmp_ok($E->getHeight(), '==', 2, '... E has a height of 2');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    
    my $C = Tree::Simple->new('C');
    isa_ok($C, 'Tree::Simple');
    
    $D->addChild($C);
    
    #    |
    #   <D>
    #   / \
    # <C> <E>
    #       \
    #       <F>
    
    cmp_ok($D->getHeight(), '==', 3, '... D has a height of 3');
    cmp_ok($E->getHeight(), '==', 2, '... E has a height of 2');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($C->getHeight(), '==', 1, '... C has a height of 1');
    
    my $B = Tree::Simple->new('B');
    isa_ok($B, 'Tree::Simple');
    
    $C->addChild($B);
    
    #      |
    #     <D>
    #     / \
    #   <C> <E>
    #   /     \
    # <B>     <F>
    
    
    cmp_ok($D->getHeight(), '==', 3, '... D has a height of 3');
    cmp_ok($E->getHeight(), '==', 2, '... E has a height of 2');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($C->getHeight(), '==', 2, '... C has a height of 2');
    cmp_ok($B->getHeight(), '==', 1, '... B has a height of 1');
    
    my $A = Tree::Simple->new('A');
    isa_ok($A, 'Tree::Simple');
    
    $B->addChild($A);
    
    #        |
    #       <D>
    #       / \
    #     <C> <E>
    #     /     \
    #   <B>     <F>
    #   /         
    # <A>         
    
    cmp_ok($D->getHeight(), '==', 4, '... D has a height of 4');
    cmp_ok($E->getHeight(), '==', 2, '... E has a height of 2');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($C->getHeight(), '==', 3, '... C has a height of 3');
    cmp_ok($B->getHeight(), '==', 2, '... B has a height of 2');
    cmp_ok($A->getHeight(), '==', 1, '... A has a height of 1');
    
    my $G = Tree::Simple->new('G');
    isa_ok($G, 'Tree::Simple');
    
    $E->insertChild(0, $G);
    
    #        |
    #       <D>
    #       / \
    #     <C> <E>
    #     /   / \
    #   <B> <G> <F>
    #   /         
    # <A>         
    
    cmp_ok($D->getHeight(), '==', 4, '... D has a height of 4');
    cmp_ok($E->getHeight(), '==', 2, '... E has a height of 2');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($G->getHeight(), '==', 1, '... G has a height of 1');
    cmp_ok($C->getHeight(), '==', 3, '... C has a height of 3');
    cmp_ok($B->getHeight(), '==', 2, '... B has a height of 2');
    cmp_ok($A->getHeight(), '==', 1, '... A has a height of 1');
    
    my $H = Tree::Simple->new('H');
    isa_ok($H, 'Tree::Simple');
    
    $G->addChild($H);
    
    #        |
    #       <D>
    #       / \
    #     <C> <E>
    #     /   / \
    #   <B> <G> <F>
    #   /     \    
    # <A>     <H>    
    
    cmp_ok($D->getHeight(), '==', 4, '... D has a height of 4');
    cmp_ok($E->getHeight(), '==', 3, '... E has a height of 3');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($G->getHeight(), '==', 2, '... G has a height of 2');
    cmp_ok($H->getHeight(), '==', 1, '... H has a height of 1');
    cmp_ok($C->getHeight(), '==', 3, '... C has a height of 3');
    cmp_ok($B->getHeight(), '==', 2, '... B has a height of 2');
    cmp_ok($A->getHeight(), '==', 1, '... A has a height of 1');

    ok($B->removeChild($A), '... removed A subtree from B tree');

    #        |
    #       <D>
    #       / \
    #     <C> <E>
    #     /   / \
    #   <B> <G> <F>
    #         \    
    #         <H> 

    cmp_ok($D->getHeight(), '==', 4, '... D has a height of 4');
    cmp_ok($E->getHeight(), '==', 3, '... E has a height of 3');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($G->getHeight(), '==', 2, '... G has a height of 2');
    cmp_ok($H->getHeight(), '==', 1, '... H has a height of 1');
    cmp_ok($C->getHeight(), '==', 2, '... C has a height of 2');
    cmp_ok($B->getHeight(), '==', 1, '... B has a height of 1');
    
    # and the removed tree is ok
    cmp_ok($A->getHeight(), '==', 1, '... A has a height of 1');
    
    ok($D->removeChild($E), '... removed E subtree from D tree');

    #        |
    #       <D>
    #       / 
    #     <C> 
    #     /     
    #   <B>

    cmp_ok($D->getHeight(), '==', 3, '... D has a height of 3');
    cmp_ok($C->getHeight(), '==', 2, '... C has a height of 2');
    cmp_ok($B->getHeight(), '==', 1, '... B has a height of 1');
    
    # and the removed trees are ok
    cmp_ok($E->getHeight(), '==', 3, '... E has a height of 3');
    cmp_ok($F->getHeight(), '==', 1, '... F has a height of 1');
    cmp_ok($G->getHeight(), '==', 2, '... G has a height of 2');
    cmp_ok($H->getHeight(), '==', 1, '... H has a height of 1');    
    
    ok($D->removeChild($C), '... removed C subtree from D tree');

    #        |
    #       <D>

    cmp_ok($D->getHeight(), '==', 1, '... D has a height of 1');
    
    # and the removed tree is ok
    cmp_ok($C->getHeight(), '==', 2, '... C has a height of 2');
    cmp_ok($B->getHeight(), '==', 1, '... B has a height of 1');      
}
