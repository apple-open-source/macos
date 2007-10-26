#!/usr/bin/perl

use strict;
use warnings;

use Test::More;

eval "use Test::Memory::Cycle 1.02";
plan skip_all => "Test::Memory::Cycle required for testing memory leaks" if $@;

plan tests => 43;

use_ok('Tree::Simple', 'use_weak_refs');

#diag "parental connections are weak";

{

    my $tree2 = Tree::Simple->new("2");
    ok($tree2->isRoot(), '... tree2 is a ROOT');    
    
    {
        my $tree1 = Tree::Simple->new("1");
        $tree1->addChild($tree2);
        ok(!$tree2->isRoot(), '... now tree2 is not a ROOT');

        weakened_memory_cycle_exists($tree2, '... there is a weakened cycle in tree2');
    }
    
    weakened_memory_cycle_ok($tree2, '... tree2 is no longer connected to tree1');
    ok($tree2->isRoot(), '... now tree2 is a ROOT again');
    ok(!defined($tree2->getParent()), '... now tree2s parent is no longer defined');    
}

#diag "expand the problem to check child connections";

{ 

    my $tree2 = Tree::Simple->new("2");
    ok($tree2->isRoot(), '... tree2 is a ROOT');  
    ok($tree2->isLeaf(), '... tree2 is a Leaf');      
    my $tree3 = Tree::Simple->new("3");  
    ok($tree3->isRoot(), '... tree3 is a ROOT');  
    ok($tree3->isLeaf(), '... tree3 is a Leaf'); 
    
    {
        my $tree1 = Tree::Simple->new("1");
        $tree1->addChild($tree2);
        ok(!$tree2->isRoot(), '... now tree2 is not a ROOT');
        $tree2->addChild($tree3);
        ok(!$tree2->isLeaf(), '... now tree2 is not a Leaf');
        ok(!$tree3->isRoot(), '... tree3 is no longer a ROOT');  
        ok($tree3->isLeaf(), '... but tree3 is still a Leaf'); 

        weakened_memory_cycle_exists($tree1, '... there is a cycle in tree1');
        weakened_memory_cycle_exists($tree2, '... there is a cycle in tree2');
        weakened_memory_cycle_exists($tree3, '... there is a cycle in tree3'); 
    }
    
    weakened_memory_cycle_exists($tree2, '... calling DESTORY on tree1 broke the connection with tree2');
    ok($tree2->isRoot(), '... now tree2 is a ROOT again');
    ok(!$tree2->isLeaf(), '... now tree2 is a not a leaf again');    
    ok(!defined($tree2->getParent()), '... now tree2s parent is no longer defined');    
    cmp_ok($tree2->getChildCount(), '==', 1, '... now tree2 has one child');    
    weakened_memory_cycle_exists($tree3, '... calling DESTORY on tree1 did not break the connection betwee tree2 and tree3');
    ok(!$tree3->isRoot(), '... now tree3 is not a ROOT');
    ok($tree3->isLeaf(), '... now tree3 is still a leaf');    
    ok(defined($tree3->getParent()), '... now tree3s parent is still defined');   
    is($tree3->getParent(), $tree2, '... now tree3s parent is still tree2');        
}

#diag "child connections are strong";
{
    my $tree1 = Tree::Simple->new("1");
    my $tree2_UID;

    {
        my $tree2 = Tree::Simple->new("2");    
        $tree1->addChild($tree2);
        $tree2_UID = $tree2->getUID();
        
        weakened_memory_cycle_exists($tree1, '... tree1 is connected to tree2');
        weakened_memory_cycle_exists($tree2, '... tree2 is connected to tree1');        
    }

    weakened_memory_cycle_exists($tree1, '... tree2 is still connected to tree1 because child connections are strong');
    is($tree1->getChild(0)->getUID(), $tree2_UID, '... tree2 is still connected to tree1');
    is($tree1->getChild(0)->getParent(), $tree1, '... tree2s parent is tree1');
    cmp_ok($tree1->getChildCount(), '==', 1, '... tree1 has a child count of 1');        
}

#diag "expand upon this issue";
{
    my $tree1 = Tree::Simple->new("1");
    my $tree2_UID;
    my $tree3 = Tree::Simple->new("3");    

    {
        my $tree2 = Tree::Simple->new("2");    
        $tree1->addChild($tree2);
        $tree2_UID = $tree2->getUID();
        $tree2->addChild($tree3);
        
        weakened_memory_cycle_exists($tree1, '... tree1 is connected to tree2');
        weakened_memory_cycle_exists($tree2, '... tree2 is connected to tree1');    
        weakened_memory_cycle_exists($tree3, '... tree3 is connected to tree2');            
    }

    weakened_memory_cycle_exists($tree1, '... tree2 is still connected to tree1 because child connections are strong');
    is($tree1->getChild(0)->getUID(), $tree2_UID, '... tree2 is still connected to tree1');
    is($tree1->getChild(0)->getParent(), $tree1, '... tree2s parent is tree1');
    cmp_ok($tree1->getChildCount(), '==', 1, '... tree1 has a child count of 1');        
    cmp_ok($tree1->getChild(0)->getChildCount(), '==', 1, '... tree2 is still connected to tree3');
    is($tree1->getChild(0)->getChild(0), $tree3, '... tree2 is still connected to tree3');    
}

