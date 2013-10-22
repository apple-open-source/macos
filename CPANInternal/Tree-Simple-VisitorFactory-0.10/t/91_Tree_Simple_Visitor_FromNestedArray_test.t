#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 58;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::FromNestedArray');
}

use Tree::Simple;

my $array_tree = [ 
            'Root', [ 
                'Child1', [
                        'GrandChild1',
                        'GrandChild2'
                        ],
                'Child2'
                ]
            ];


can_ok("Tree::Simple::Visitor::FromNestedArray", 'new');

{ # check normal behavior
    my $visitor = Tree::Simple::Visitor::FromNestedArray->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FromNestedArray');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setArrayTree');
    $visitor->setArrayTree($array_tree);
    
    can_ok($visitor, 'visit');
    
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    $tree->accept($visitor);
    
    my $root = $tree->getChild(0);
    is($root->getNodeValue(), 'Root', '... got the value we expected from Root');
    cmp_ok($root->getChildCount(), '==', 2, '... Root has 2 children');
    
        my ($child1, $child2) = $root->getAllChildren();
        is($child1->getNodeValue(), 'Child1', '... got the value we expected from Child1');
        cmp_ok($child1->getChildCount(), '==', 2, '... Child1 has 2 children');
        
            my ($grandchild1, $grandchild2) = $child1->getAllChildren();
            is($grandchild1->getNodeValue(), 'GrandChild1', '... got the value we expected from GrandChild1');
            ok($grandchild1->isLeaf(), '... GrandChild1 is a leaf node');
            
            is($grandchild2->getNodeValue(), 'GrandChild2', '... got the value we expected from GrandChild2');
            ok($grandchild2->isLeaf(), '... GrandChild2 is a leaf node');
        
        is($child2->getNodeValue(), 'Child2', '... got the value we expected from Child2');
        ok($child2->isLeaf(), '... Child2 is a leaf node');	    
}

{ # check includeTrunk behavior
    my $visitor = Tree::Simple::Visitor::FromNestedArray->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FromNestedArray');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setArrayTree');
    $visitor->setArrayTree($array_tree);

    can_ok($visitor, 'includeTrunk');
    $visitor->includeTrunk(1);
    
    can_ok($visitor, 'visit');
    
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    $tree->accept($visitor);
    
    my $root = $tree;
    is($root->getNodeValue(), 'Root', '... got the value we expected from Root');
    cmp_ok($root->getChildCount(), '==', 2, '... Root has 2 children');
    
        my ($child1, $child2) = $root->getAllChildren();
        is($child1->getNodeValue(), 'Child1', '... got the value we expected from Child1');
        cmp_ok($child1->getChildCount(), '==', 2, '... Child1 has 2 children');
        
            my ($grandchild1, $grandchild2) = $child1->getAllChildren();
            is($grandchild1->getNodeValue(), 'GrandChild1', '... got the value we expected from GrandChild1');
            ok($grandchild1->isLeaf(), '... GrandChild1 is a leaf node');
            
            is($grandchild2->getNodeValue(), 'GrandChild2', '... got the value we expected from GrandChild2');
            ok($grandchild2->isLeaf(), '... GrandChild2 is a leaf node');
        
        is($child2->getNodeValue(), 'Child2', '... got the value we expected from Child2');
        ok($child2->isLeaf(), '... Child2 is a leaf node');	
}

{ # check nodeFilter behavior
    my $visitor = Tree::Simple::Visitor::FromNestedArray->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FromNestedArray');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setArrayTree');
    $visitor->setArrayTree($array_tree);

    can_ok($visitor, 'setNodeFilter');
    $visitor->setNodeFilter(sub {
        my ($node) = @_;
        return uc($node);
    });
    
    can_ok($visitor, 'visit');
    
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    $tree->accept($visitor);
    
    my $root = $tree->getChild(0);
    is($root->getNodeValue(), 'ROOT', '... got the value we expected from Root');
    cmp_ok($root->getChildCount(), '==', 2, '... Root has 2 children');
    
        my ($child1, $child2) = $root->getAllChildren();
        is($child1->getNodeValue(), 'CHILD1', '... got the value we expected from Child1');
        cmp_ok($child1->getChildCount(), '==', 2, '... Child1 has 2 children');
        
            my ($grandchild1, $grandchild2) = $child1->getAllChildren();
            is($grandchild1->getNodeValue(), 'GRANDCHILD1', '... got the value we expected from GrandChild1');
            ok($grandchild1->isLeaf(), '... GrandChild1 is a leaf node');
            
            is($grandchild2->getNodeValue(), 'GRANDCHILD2', '... got the value we expected from GrandChild2');
            ok($grandchild2->isLeaf(), '... GrandChild2 is a leaf node');
        
        is($child2->getNodeValue(), 'CHILD2', '... got the value we expected from Child2');
        ok($child2->isLeaf(), '... Child2 is a leaf node');
}

{
    my $visitor = Tree::Simple::Visitor::FromNestedArray->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FromNestedArray');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    # check visit
    throws_ok {
        $visitor->visit();
    } qr/Insufficient Arguments/, '... got the error we expected';  
    
    throws_ok {
        $visitor->visit("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';                           

    throws_ok {
        $visitor->visit([]);
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
    throws_ok {
        $visitor->visit(bless({}, "Fail"));
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
    # check setHashTree
    throws_ok {
        $visitor->setArrayTree();
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
    throws_ok {
        $visitor->setArrayTree("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
    throws_ok {
        $visitor->setArrayTree([]);
    } qr/Insufficient Arguments/, '... got the error we expected';     

    throws_ok {
        $visitor->setArrayTree([[]]);
    } qr/Incorrect Object Type/, '... got the error we expected';

    throws_ok {
        $visitor->setArrayTree(['root', 'Fail']);
    } qr/Incorrect Object Type/, '... got the error we expected';

    $visitor->setArrayTree(['root', [[]]]);
    
    throws_ok {
        $visitor->visit(Tree::Simple->new(Tree::Simple->ROOT));
    } qr/Incorrect Object Type/, '... got the error we expected';    

}
