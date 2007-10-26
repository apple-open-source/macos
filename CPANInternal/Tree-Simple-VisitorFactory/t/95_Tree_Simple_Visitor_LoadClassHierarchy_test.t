#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 50;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::LoadClassHierarchy');
}

use Tree::Simple;

can_ok("Tree::Simple::Visitor::LoadClassHierarchy", 'new');

# ---------------------------
# classic diamond inheritance
# ---------------------------
#   A   B
#  / \ /
# C   D
#  \ /
#   E
# ---------------------------
# modeled as this tree
# ---------------------------
# A   A   B
#  \   \ /
#   C   D
#    \ /
#     E
# ---------------------------
{
    package A;
    package B;
    package C; @C::ISA = ('A');
    package D; @D::ISA = ('A', 'B');
    package E; @E::ISA = ('C', 'D');
}

{
    my $visitor = Tree::Simple::Visitor::LoadClassHierarchy->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadClassHierarchy');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setClass');
    $visitor->setClass('E');   
             
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    isa_ok($tree, 'Tree::Simple');             
             
    can_ok($visitor, 'visit');         
    $tree->accept($visitor);    
    
    my $current = $tree->getChild(0);
    is($current->getNodeValue(), 'E', '... got the value we expected');
    is($current->getChild(0)->getNodeValue(), 'C', '... got the value we expected');                    
    is($current->getChild(0)->getChild(0)->getNodeValue(), 'A', '... got the value we expected');                    
    is($current->getChild(1)->getNodeValue(), 'D', '... got the value we expected');                    
    is($current->getChild(1)->getChild(0)->getNodeValue(), 'A', '... got the value we expected');                    
    is($current->getChild(1)->getChild(1)->getNodeValue(), 'B', '... got the value we expected');                    
}

{
    my $visitor = Tree::Simple::Visitor::LoadClassHierarchy->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadClassHierarchy');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setClass');
    $visitor->setClass('E');   
             
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    isa_ok($tree, 'Tree::Simple');             
             
    can_ok($visitor, 'includeTrunk');         
    $visitor->includeTrunk(1);
    
    can_ok($visitor, 'visit');         
    $tree->accept($visitor);    
    
    my $current = $tree;
    is($current->getNodeValue(), 'E', '... got the value we expected');
    is($current->getChild(0)->getNodeValue(), 'C', '... got the value we expected');                    
    is($current->getChild(0)->getChild(0)->getNodeValue(), 'A', '... got the value we expected');                    
    is($current->getChild(1)->getNodeValue(), 'D', '... got the value we expected');                    
    is($current->getChild(1)->getChild(0)->getNodeValue(), 'A', '... got the value we expected');                    
    is($current->getChild(1)->getChild(1)->getNodeValue(), 'B', '... got the value we expected');                    
}

{
    my $visitor = Tree::Simple::Visitor::LoadClassHierarchy->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadClassHierarchy');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setClass');
    $visitor->setClass('E');   
             
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    isa_ok($tree, 'Tree::Simple');   
    
    can_ok($visitor, 'setNodeFilter');         
    $visitor->setNodeFilter(sub { "Package::" . $_[0] });                        
             
    can_ok($visitor, 'visit');         
    $tree->accept($visitor);    
    
    my $current = $tree->getChild(0);
    is($current->getNodeValue(), 'Package::E', '... got the value we expected');
    is($current->getChild(0)->getNodeValue(), 'Package::C', '... got the value we expected');                    
    is($current->getChild(0)->getChild(0)->getNodeValue(), 'Package::A', '... got the value we expected');                    
    is($current->getChild(1)->getNodeValue(), 'Package::D', '... got the value we expected');                    
    is($current->getChild(1)->getChild(0)->getNodeValue(), 'Package::A', '... got the value we expected');                    
    is($current->getChild(1)->getChild(1)->getNodeValue(), 'Package::B', '... got the value we expected');                    
}

{
    package One;
    sub new {}
    sub one {}
    
    package Two;
    @Two::ISA = ('One');
    sub two {}
    
    package Three; 
    @Three::ISA = ('Two');
    sub three {}
}

{
    my $visitor = Tree::Simple::Visitor::LoadClassHierarchy->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadClassHierarchy');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setClass');
    $visitor->setClass('Three');   
             
    my $tree = Tree::Simple->new(Tree::Simple->ROOT);
    isa_ok($tree, 'Tree::Simple');   
    
    can_ok($visitor, 'includeMethods');         
    $visitor->includeMethods(1);                        
             
    can_ok($visitor, 'visit');         
    $tree->accept($visitor);            
    
    my $current = $tree->getChild(0);
    is($current->getNodeValue(), 'Three', '... got the value we expected');    
    is($current->getChild(0)->getNodeValue(), 'three', '... got the value we expected');                    
    
    is($current->getChild(1)->getNodeValue(), 'Two', '... got the value we expected');                    
    is($current->getChild(1)->getChild(0)->getNodeValue(), 'two', '... got the value we expected');                    
    
    is($current->getChild(1)->getChild(1)->getNodeValue(), 'One', '... got the value we expected');                    
    is($current->getChild(1)->getChild(1)->getChild(0)->getNodeValue(), 'new', '... got the value we expected');                    
    is($current->getChild(1)->getChild(1)->getChild(1)->getNodeValue(), 'one', '... got the value we expected');                    
}
