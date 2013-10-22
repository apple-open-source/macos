#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 46;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::FindByPath');
}

use Tree::Simple;

my $first_search = Tree::Simple->new("1.2.2");
isa_ok($first_search, 'Tree::Simple');

my $second_search = Tree::Simple->new("3.2.1");
isa_ok($second_search, 'Tree::Simple');

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

can_ok("Tree::Simple::Visitor::FindByPath", 'new');

my $visitor = Tree::Simple::Visitor::FindByPath->new();
isa_ok($visitor, 'Tree::Simple::Visitor::FindByPath');
isa_ok($visitor, 'Tree::Simple::Visitor');

can_ok($visitor, 'setSearchPath');
can_ok($visitor, 'visit');
can_ok($visitor, 'getResult');

# test our first search path
$visitor->setSearchPath(qw(1 1.2 1.2.2));
$tree->accept($visitor);
is($visitor->getResult(), $first_search, '... this should be what we got back');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 4, '... go four results (including root)');
    is($results[0], $tree, '... got the right first result');
    is($results[1], $tree->getChild(0), '... got the right next result');    
    is($results[2], $tree->getChild(0)->getChild(1), '... got the right next result');  
    is($results[3], $first_search, '... got the right next result');            
}

# test our first failing search path
$visitor->setSearchPath(qw(1 1.2 1.2.3));
$tree->accept($visitor);
ok(!defined($visitor->getResult()), '... match failed so we get undef back');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 3, '... go three results (including root)');
    is($results[0], $tree, '... got the right first result');
    is($results[1], $tree->getChild(0), '... got the right next result');    
    is($results[2], $tree->getChild(0)->getChild(1), '... got the right next result');            
}

# test our next failing search path
$visitor->setSearchPath(qw(1 1.5 1.2.3));
$tree->accept($visitor);
ok(!defined($visitor->getResult()), '... match failed so we get undef back');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 2, '... go two results (including root)');
    is($results[0], $tree, '... got the right first result');
    is($results[1], $tree->getChild(0), '... got the right next result');    
}

# test our next failing search path
$visitor->setSearchPath(qw(100 1.5 1.2.3));
$tree->accept($visitor);
ok(!defined($visitor->getResult()), '... match failed so we get undef back');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 0, '... go no results (including root)');
}

# add a node filter
can_ok($visitor, 'setNodeFilter');
$visitor->setNodeFilter(sub { "Tree_" . $_[0]->getNodeValue() });

# test our new search path with filter
$visitor->setSearchPath(qw(Tree_3 Tree_3.2 Tree_3.2.1));
$tree->accept($visitor);
is($visitor->getResult(), $second_search, '... this should be what we got back');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 4, '... go four results (including root)');
    is($results[0], $tree, '... got the right first result');
    is($results[1], $tree->getChild(2), '... got the right next result');    
    is($results[2], $tree->getChild(2)->getChild(1), '... got the right next result');  
    is($results[3], $second_search, '... got the right next result');            
}

# use the trunk
can_ok($visitor, 'includeTrunk');
$visitor->includeTrunk(1);

# test path failure
$visitor->setSearchPath(qw(Tree_root Tree_1 Tree_5 Tree_35));
$tree->accept($visitor);
ok(!defined($visitor->getResult()), '... should fail, and we get back undef');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 2, '... we should have gotten the root, and 1');
    is($results[0], $tree, '... we should not have gotten farther than the 1');
    is($results[1], $tree->getChild(0), '... we should not have gotten farther than the 1');
}

# test total path failure
$visitor->setSearchPath(qw(8 5 35));
$tree->accept($visitor);
ok(!defined($visitor->getResult()), '... should fail, and we get back undef');

{
    my @results = $visitor->getResults();
    is(scalar(@results), 0, '... we should have gotten nothing at all');
}

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
    $visitor->setSearchPath();
} qr/Insufficient Arguments/, '... this should die';
