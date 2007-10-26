#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 33;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::ToNestedHash');
}

use Tree::Simple;

my $tree = Tree::Simple->new("Root")
                ->addChildren(
                    Tree::Simple->new("Child1")
                        ->addChildren(
                            Tree::Simple->new("GrandChild1"),                
                            Tree::Simple->new("GrandChild2")
                        ),
                    Tree::Simple->new("Child2"),
                );
isa_ok($tree, 'Tree::Simple');

can_ok("Tree::Simple::Visitor::ToNestedHash", 'new');
    
{
    my $visitor = Tree::Simple::Visitor::ToNestedHash->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::ToNestedHash');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'visit');
    can_ok($visitor, 'getResults');    
        
    $tree->accept($visitor);
    is_deeply($visitor->getResults(),
            { 'Child1' => { 'GrandChild1' => {}, 'GrandChild2' => {}}, 'Child2' => {}},
            '... got the whole tree');
}

{
    my $visitor = Tree::Simple::Visitor::ToNestedHash->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::ToNestedHash');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'includeTrunk');
    can_ok($visitor, 'visit');
    can_ok($visitor, 'getResults');    
        
    $visitor->includeTrunk(1);
    $tree->accept($visitor);
    is_deeply($visitor->getResults(),
            { 'Root' => { 'Child1' => { 'GrandChild1' => {}, 'GrandChild2' => {}}, 'Child2' => {}}},
            '... got the whole tree');
}

{
    my $visitor = Tree::Simple::Visitor::ToNestedHash->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::ToNestedHash');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'visit');
    can_ok($visitor, 'getResults');    
    can_ok($visitor, 'setNodeFilter');                                    
    
    $visitor->setNodeFilter(sub {
        return uc($_[0]->getNodeValue());
    });    
        
    $tree->accept($visitor);
    is_deeply($visitor->getResults(),
            { 'CHILD1' => { 'GRANDCHILD1' => {}, 'GRANDCHILD2' => {}}, 'CHILD2' => {}},
            '... got the whole tree');
}

{
    my $visitor = Tree::Simple::Visitor::ToNestedHash->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::ToNestedHash');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'includeTrunk');    
    can_ok($visitor, 'visit');
    can_ok($visitor, 'getResults');    
    can_ok($visitor, 'setNodeFilter');                                    
    
    $visitor->setNodeFilter(sub {
        return uc($_[0]->getNodeValue());
    });    
    $visitor->includeTrunk(1);        
    $tree->accept($visitor);
    is_deeply($visitor->getResults(),
            { 'ROOT' => { 'CHILD1' => { 'GRANDCHILD1' => {}, 'GRANDCHILD2' => {}}, 'CHILD2' => {}}},
            '... got the whole tree');
}

{
    my $visitor = Tree::Simple::Visitor::ToNestedHash->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::ToNestedHash');
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
}
