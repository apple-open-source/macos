#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 49;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::FindByNodeValue');
    use_ok('Tree::Simple::Visitor::BreadthFirstTraversal');
}

use Tree::Simple;

my $first_search = Tree::Simple->new("1.2.2");
isa_ok($first_search, 'Tree::Simple');

my $first_search_NodeValue = '1.2.2';

my $second_search = Tree::Simple->new("3.2.1");
isa_ok($second_search, 'Tree::Simple');

my $second_search_NodeValue = '3.2.1';
my $second_search_UID = $second_search->getUID();

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.1"),
                                            Tree::Simple->new("1.2")
                                                        ->addChildren(
                                                            Tree::Simple->new("1.2.1"),
                                                            $first_search
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
                                            Tree::Simple->new("3.2")->addChild($second_search),
                                            Tree::Simple->new("3.3")                                                                                                
                                        ),                            
                            Tree::Simple->new("4")                                                        
                                        ->addChildren(
                                            Tree::Simple->new("4.1")
                                        )                            
                       );
isa_ok($tree, 'Tree::Simple');

can_ok("Tree::Simple::Visitor::FindByNodeValue", 'new');

# check the normal behavior
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'searchForNodeValue');
    can_ok($visitor, 'visit');
    can_ok($visitor, 'getResult');
    
    $visitor->searchForNodeValue($first_search_NodeValue);
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(defined($match), '... we got a result');
    is($match, $first_search, '... and it is our first search tree');
}

# test the node filter and make it fail
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    $visitor->searchForNodeValue($first_search_NodeValue);
    # make our search fail
    $visitor->setNodeFilter(sub {
        my ($tree) = @_;
        return $tree->getNodeValue() ne "1.2.2";
    });
    
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(!defined($match), '... match failed as expected');
}

# test the second match
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    $visitor->searchForNodeValue($second_search_NodeValue);
    # make our search succed
    $visitor->setNodeFilter(sub {
        my ($tree) = @_;
        return $tree->getUID() eq $second_search_UID;
    });
    
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(defined($match), '... match succedded as expected');
    is($match, $second_search, '... and it is our second search tree');    
}

# check the normal behavior with includeTrunk
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'includeTrunk');
    $visitor->includeTrunk(1);
    
    $visitor->searchForNodeValue($tree->getNodeValue());
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(defined($match), '... we got a result');
    is($match, $tree, '... and it is our base tree');
}

# check the traversal method behavior
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'setTraversalMethod');
    $visitor->setTraversalMethod(Tree::Simple::Visitor::BreadthFirstTraversal->new());    
    
    $visitor->searchForNodeValue($first_search_NodeValue);
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(defined($match), '... we got a result');
    is($match, $first_search, '... and it is our first search tree');
}

# check the traversal method behavior with includeTrunk
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    $visitor->setTraversalMethod(Tree::Simple::Visitor::BreadthFirstTraversal->new());    
    $visitor->includeTrunk(1);
    $visitor->searchForNodeValue($tree->getNodeValue());
    
    $tree->accept($visitor);
    
    my $match = $visitor->getResult();
    ok(defined($match), '... we got a result');
    is($match, $tree, '... and it is our base tree');
}


# check errors
{
    my $visitor = Tree::Simple::Visitor::FindByNodeValue->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByNodeValue');
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
    
    # check UID
    throws_ok {
        $visitor->searchForNodeValue();
    } qr/Insufficient Arguments/, '... got the error we expected';      
    
    # try to visit without a UID
    throws_ok {
        $visitor->visit($tree);
    } qr/Illegal Operation/, '... got the error we expected';     
    
    # check setTraversalMethod
    throws_ok {
        $visitor->setTraversalMethod();
    } qr/Insufficient Arguments/, '... got the error we expected';  
    
    throws_ok {
        $visitor->setTraversalMethod("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';                           

    throws_ok {
        $visitor->setTraversalMethod([]);
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
    throws_ok {
        $visitor->setTraversalMethod(bless({}, "Fail"));
    } qr/Insufficient Arguments/, '... got the error we expected';  
    
    throws_ok {
        $visitor->searchForNodeValue();
    } qr/Insufficient Arguments/, '... got the error we expected';    
    
    # test some edge cases
    $visitor->searchForNodeValue($first_search_NodeValue);
    
    $visitor->setNodeFilter(sub { die "Nothing really" });
    throws_ok {
        $visitor->visit($tree);
    } qr/Nothing really/, '... got the error we expected';  
    
    $visitor->setNodeFilter(sub { die bless({}, "NothingReally") });
    throws_ok {
        $visitor->visit($tree);
    } "NothingReally", '... got the error we expected';                    
        
}

