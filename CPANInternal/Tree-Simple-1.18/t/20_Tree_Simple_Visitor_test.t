#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 37;
use Test::Exception;

BEGIN { 
	use_ok('Tree::Simple::Visitor'); 	
};

use Tree::Simple;

my $SIMPLE_SUB = sub { "test sub" };
# execute this otherwise Devel::Cover gives odd stats
$SIMPLE_SUB->();

# check that we have a constructor
can_ok("Tree::Simple::Visitor", 'new');

# -----------------------------------------------
# test the new style interface
# -----------------------------------------------

my $visitor = Tree::Simple::Visitor->new();
isa_ok($visitor, 'Tree::Simple::Visitor');

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
					   ->addChildren(
							Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.1"),
                                            Tree::Simple->new("1.2")
                                                        ->addChild(Tree::Simple->new("1.2.1")),
                                            Tree::Simple->new("1.3")                                            
                                        ),
							Tree::Simple->new("2"),
							Tree::Simple->new("3"),							
					   );
isa_ok($tree, 'Tree::Simple');

$tree->accept($visitor);

can_ok($visitor, 'getResults');
is_deeply(
        [ $visitor->getResults() ],
        [ qw(1 1.1 1.2 1.2.1 1.3 2 3)],
        '... got what we expected');

can_ok($visitor, 'setNodeFilter');

my $node_filter = sub { return "_" . $_[0]->getNodeValue() };
$visitor->setNodeFilter($node_filter);

can_ok($visitor, 'getNodeFilter');
is($visitor->getNodeFilter(), "$node_filter", '... got back what we put in');

# visit the tree again to get new results now
$tree->accept($visitor);

is_deeply(
        scalar $visitor->getResults(),
        [ qw(_1 _1.1 _1.2 _1.2.1 _1.3 _2 _3)],
        '... got what we expected');
        
# test some exceptions

throws_ok {
    $visitor->setNodeFilter();        
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $visitor->setNodeFilter([]);        
} qr/Insufficient Arguments/, '... this should die';

# -----------------------------------------------
# test the old style interface for backwards 
# compatability
# -----------------------------------------------

# and that our RECURSIVE constant is properly defined
can_ok("Tree::Simple::Visitor", 'RECURSIVE');
# and that our CHILDREN_ONLY constant is properly defined
can_ok("Tree::Simple::Visitor", 'CHILDREN_ONLY');

# no depth
my $visitor1 = Tree::Simple::Visitor->new($SIMPLE_SUB);
isa_ok($visitor1, 'Tree::Simple::Visitor');

# children only
my $visitor2 = Tree::Simple::Visitor->new($SIMPLE_SUB, Tree::Simple::Visitor->CHILDREN_ONLY);
isa_ok($visitor2, 'Tree::Simple::Visitor');

# recursive
my $visitor3 = Tree::Simple::Visitor->new($SIMPLE_SUB, Tree::Simple::Visitor->RECURSIVE);
isa_ok($visitor3, 'Tree::Simple::Visitor');

# -----------------------------------------------
# test constructor exceptions
# -----------------------------------------------

# we pass a bad depth (string)
throws_ok {
	my $test = Tree::Simple::Visitor->new($SIMPLE_SUB, "Fail")
} qr/Insufficient Arguments \: Depth arguement must be either RECURSIVE or CHILDREN_ONLY/, 
   '... we are expecting this error';
   
# we pass a bad depth (numeric)
throws_ok {
	my $test = Tree::Simple::Visitor->new($SIMPLE_SUB, 100)
} qr/Insufficient Arguments \: Depth arguement must be either RECURSIVE or CHILDREN_ONLY/, 
   '... we are expecting this error';     

# we pass a non-ref func argument
throws_ok {
	my $test = Tree::Simple::Visitor->new("Fail");
} qr/Insufficient Arguments \: filter function argument must be a subroutine reference/,
   '... we are expecting this error';

# we pass a non-code-ref func arguement   
throws_ok {
	my $test = Tree::Simple::Visitor->new([]);
} qr/Insufficient Arguments \: filter function argument must be a subroutine reference/,
   '... we are expecting this error';   

# -----------------------------------------------
# test other exceptions
# -----------------------------------------------

# and make sure we can call the visit method
can_ok($visitor1, 'visit');

# test no arg
throws_ok {
	$visitor1->visit();
} qr/Insufficient Arguments \: You must supply a valid Tree\:\:Simple object/,
   '... we are expecting this error'; 
   
# test non-ref arg
throws_ok {
	$visitor1->visit("Fail");
} qr/Insufficient Arguments \: You must supply a valid Tree\:\:Simple object/,
   '... we are expecting this error'; 	 
   
# test non-object ref arg
throws_ok {
	$visitor1->visit([]);
} qr/Insufficient Arguments \: You must supply a valid Tree\:\:Simple object/,
   '... we are expecting this error'; 	   
   
my $BAD_OBJECT = bless({}, "Test");   
   
# test non-Tree::Simple object arg
throws_ok {
	$visitor1->visit($BAD_OBJECT);
} qr/Insufficient Arguments \: You must supply a valid Tree\:\:Simple object/,
   '... we are expecting this error'; 	   
   

# -----------------------------------------------
# Test accept & visit
# -----------------------------------------------
# Note: 
# this test could be made more robust by actually
# getting results and testing them from the 
# Visitor object. But for right now it is good
# enough to have the code coverage, and know
# all the peices work.
# -----------------------------------------------

# now make a tree
my $tree1 = Tree::Simple->new(Tree::Simple->ROOT)
					   ->addChildren(
							Tree::Simple->new("1.0"),
							Tree::Simple->new("2.0"),
							Tree::Simple->new("3.0"),							
					   );
isa_ok($tree1, 'Tree::Simple');

cmp_ok($tree1->getChildCount(), '==', 3, '... there are 3 children here');

# and pass the visitor1 to accept
lives_ok {
	$tree1->accept($visitor1);
} '.. this passes fine';

# and pass the visitor2 to accept
lives_ok {
	$tree1->accept($visitor2);
} '.. this passes fine';

# and pass the visitor3 to accept
lives_ok {
	$tree1->accept($visitor3);
} '.. this passes fine';

# ----------------------------------------------------
# test some misc. weirdness to get the coverage up :P
# ----------------------------------------------------

# check that includeTrunk works as we expect it to
{
    my $visitor = Tree::Simple::Visitor->new();
    ok(!$visitor->includeTrunk(), '... this should be false right now');

    $visitor->includeTrunk("true");
    ok($visitor->includeTrunk(), '... this should be true now');
    
    $visitor->includeTrunk(undef);
    ok($visitor->includeTrunk(), '... this should be true still');
    
    $visitor->includeTrunk("");
    ok(!$visitor->includeTrunk(), '... this should be false again');
}

# check that clearNodeFilter works as we expect it to
{
    my $visitor = Tree::Simple::Visitor->new();
    
    my $filter = sub { "filter" };
    
    $visitor->setNodeFilter($filter);
    is($visitor->getNodeFilter(), $filter, 'our node filter is set correctly');
    
    $visitor->clearNodeFilter();
    ok(! defined($visitor->getNodeFilter()), '... our node filter has now been undefined'); 
}


