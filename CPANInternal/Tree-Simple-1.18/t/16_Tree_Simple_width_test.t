#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 77;

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
    
    cmp_ok($D->getWidth(), '==', 1, '... D has a width of 1');
    
    my $E = Tree::Simple->new('E');
    isa_ok($E, 'Tree::Simple');
    
    $D->addChild($E);
    
    #   |
    #  <D>
    #    \
    #    <E>
    
    cmp_ok($D->getWidth(), '==', 1, '... D has a width of 1');
    cmp_ok($E->getWidth(), '==', 1, '... E has a width of 1');
    
    my $F = Tree::Simple->new('F');
    isa_ok($F, 'Tree::Simple');
    
    $E->addChild($F);
    
    #   |
    #  <D>
    #    \
    #    <E>
    #      \
    #      <F>
    
    cmp_ok($D->getWidth(), '==', 1, '... D has a width of 1');
    cmp_ok($E->getWidth(), '==', 1, '... E has a width of 1');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    
    my $C = Tree::Simple->new('C');
    isa_ok($C, 'Tree::Simple');
    
    $D->addChild($C);
    
    #    |
    #   <D>
    #   / \
    # <C> <E>
    #       \
    #       <F>
    
    cmp_ok($D->getWidth(), '==', 2, '... D has a width of 2');
    cmp_ok($E->getWidth(), '==', 1, '... E has a width of 1');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    
    my $B = Tree::Simple->new('B');
    isa_ok($B, 'Tree::Simple');
    
    $D->addChild($B);
    
    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #             \
    #             <F>
    
    
    cmp_ok($D->getWidth(), '==', 3, '... D has a width of 3');
    cmp_ok($E->getWidth(), '==', 1, '... E has a width of 1');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
        
    
    my $A = Tree::Simple->new('A');
    isa_ok($A, 'Tree::Simple');
    
    $E->addChild($A);
    
    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #           / \
    #         <A> <F>       
    
    cmp_ok($D->getWidth(), '==', 4, '... D has a width of 4');
    cmp_ok($E->getWidth(), '==', 2, '... E has a width of 2');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    cmp_ok($A->getWidth(), '==', 1, '... A has a width of 1');
    
    my $G = Tree::Simple->new('G');
    isa_ok($G, 'Tree::Simple');
    
    $E->insertChild(1, $G);
    
    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #          / | \
    #       <A> <G> <F>         
    
    cmp_ok($D->getWidth(), '==', 5, '... D has a width of 5');
    cmp_ok($E->getWidth(), '==', 3, '... E has a width of 3');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($G->getWidth(), '==', 1, '... G has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    cmp_ok($A->getWidth(), '==', 1, '... A has a width of 1');
    
    my $H = Tree::Simple->new('H');
    isa_ok($H, 'Tree::Simple');
    
    $G->addChild($H);
    
    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #          / | \
    #       <A> <G> <F> 
    #            |
    #           <H>    
    
    cmp_ok($D->getWidth(), '==', 5, '... D has a width of 5');
    cmp_ok($E->getWidth(), '==', 3, '... E has a width of 3');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($G->getWidth(), '==', 1, '... G has a width of 1');
    cmp_ok($H->getWidth(), '==', 1, '... H has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    cmp_ok($A->getWidth(), '==', 1, '... A has a width of 1');
    
    my $I = Tree::Simple->new('I');
    isa_ok($I, 'Tree::Simple');
    
    $G->addChild($I);
    
    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #          / | \
    #       <A> <G> <F> 
    #            | \
    #           <H> <I>   
    
    cmp_ok($D->getWidth(), '==', 6, '... D has a width of 6');
    cmp_ok($E->getWidth(), '==', 4, '... E has a width of 4');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($G->getWidth(), '==', 2, '... G has a width of 2');
    cmp_ok($H->getWidth(), '==', 1, '... H has a width of 1');
    cmp_ok($I->getWidth(), '==', 1, '... I has a width of 1');    
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    cmp_ok($A->getWidth(), '==', 1, '... A has a width of 1');      

    ok($E->removeChild($A), '... removed A subtree from B tree');

    #        |
    #       <D>
    #      / | \
    #   <B> <C> <E>
    #            | \
    #           <G> <F> 
    #            | \
    #           <H> <I>  

    cmp_ok($D->getWidth(), '==', 5, '... D has a width of 5');
    cmp_ok($E->getWidth(), '==', 3, '... E has a width of 3');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($G->getWidth(), '==', 2, '... G has a width of 2');
    cmp_ok($H->getWidth(), '==', 1, '... H has a width of 1');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 2');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    
    # and the removed tree is ok
    cmp_ok($A->getWidth(), '==', 1, '... A has a width of 1');
    
    ok($D->removeChild($E), '... removed E subtree from D tree');

    #        |
    #       <D>
    #      / | 
    #   <B> <C>

    cmp_ok($D->getWidth(), '==', 2, '... D has a width of 2');
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    
    # and the removed trees are ok
    cmp_ok($E->getWidth(), '==', 3, '... E has a width of 3');
    cmp_ok($F->getWidth(), '==', 1, '... F has a width of 1');
    cmp_ok($G->getWidth(), '==', 2, '... G has a width of 2');
    cmp_ok($H->getWidth(), '==', 1, '... H has a width of 1');    
    
    ok($D->removeChild($C), '... removed C subtree from D tree');

    #        |
    #       <D>
    #      /  
    #   <B> 

    cmp_ok($D->getWidth(), '==', 1, '... D has a width of 1');
    cmp_ok($B->getWidth(), '==', 1, '... B has a width of 1');
    
    # and the removed tree is ok
    cmp_ok($C->getWidth(), '==', 1, '... C has a width of 1');
      
}
