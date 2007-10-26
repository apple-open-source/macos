#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 48;

## ----------------------------------------------------------------------------
# NOTE:
# This specifically tests the details of the cloning functions
## ----------------------------------------------------------------------------

use Tree::Simple;

my $tree = Tree::Simple->new(Tree::Simple->ROOT);
isa_ok($tree, 'Tree::Simple');

my $test = "test";

my $SCALAR_REF = \$test;
my $REF_TO_REF = \$SCALAR_REF;
my $ARRAY_REF = [ 1, 2, 3, 4 ];
my $HASH_REF = { one => 1, two => 2 };
my $CODE_REF = sub { "code ref test" };
my $REGEX_REF = qr/^reg-ex ref/;
my $SUB_TREE = Tree::Simple->new("sub tree test");
my $MISC_OBJECT = bless({}, "Misc");

$tree->addChildren(
		Tree::Simple->new("non-ref"),	
		Tree::Simple->new($SCALAR_REF),	
		Tree::Simple->new($ARRAY_REF),
		Tree::Simple->new($HASH_REF),
		Tree::Simple->new($CODE_REF),
		Tree::Simple->new($REGEX_REF),
		Tree::Simple->new($MISC_OBJECT),
		Tree::Simple->new($SUB_TREE),
		Tree::Simple->new($REF_TO_REF)        
		);

my $clone = $tree->clone();
isa_ok($clone, 'Tree::Simple');

# make sure all the parentage is correct
is($clone->getParent(), Tree::Simple->ROOT, '... the clones parent is a root');

for my $child ($clone->getAllChildren()) {
    is($child->getParent(), $clone, '... the clones childrens parent should be our clone');
}

isnt($clone, $tree, '... these should be refs');

is($clone->getChild(0)->getNodeValue(), $tree->getChild(0)->getNodeValue(), '... these should be the same value');

# they should both be scalar refs
is(ref($clone->getChild(1)->getNodeValue()), "SCALAR", '... these should be scalar refs');
is(ref($tree->getChild(1)->getNodeValue()), "SCALAR", '... these should be scalar refs');
# but different ones
isnt($clone->getChild(1)->getNodeValue(), $tree->getChild(1)->getNodeValue(), 
	'... these should be different scalar refs');
# with the same value
is(${$clone->getChild(1)->getNodeValue()}, ${$tree->getChild(1)->getNodeValue()}, 
	'... these should be the same value');
	
# they should both be array refs
is(ref($clone->getChild(2)->getNodeValue()), "ARRAY", '... these should be array refs');
is(ref($tree->getChild(2)->getNodeValue()), "ARRAY", '... these should be array refs');
# but different ones
isnt($clone->getChild(2)->getNodeValue(), $tree->getChild(2)->getNodeValue(), 
	'... these should be different array refs');	
# with the same value	
is_deeply(
    $clone->getChild(2)->getNodeValue(), 
    $tree->getChild(2)->getNodeValue(), 
	'... these should have the same contents');
	
# they should both be hash refs
is(ref($clone->getChild(3)->getNodeValue()), "HASH", '... these should be hash refs');
is(ref($tree->getChild(3)->getNodeValue()), "HASH", '... these should be hash refs');
# but different ones
isnt($clone->getChild(3)->getNodeValue(), $tree->getChild(3)->getNodeValue(), 
	'... these should be different hash refs');	
# with the same value	
is_deeply(
    $clone->getChild(3)->getNodeValue(), 
    $tree->getChild(3)->getNodeValue(), 
	'... these should have the same contents');	

# they should both be code refs
is(ref($clone->getChild(4)->getNodeValue()), "CODE", '... these should be code refs');
is(ref($tree->getChild(4)->getNodeValue()), "CODE", '... these should be code refs');
# and still the same
is($clone->getChild(4)->getNodeValue(), $tree->getChild(4)->getNodeValue(), 
	'... these should be the same code refs');	
is($clone->getChild(4)->getNodeValue()->(), $CODE_REF->(), '... this is equal');

# they should both be reg-ex refs
is(ref($clone->getChild(5)->getNodeValue()), "Regexp", '... these should be reg-ex refs');
is(ref($tree->getChild(5)->getNodeValue()), "Regexp", '... these should be reg-ex refs');
# and still the same
is($clone->getChild(5)->getNodeValue(), $tree->getChild(5)->getNodeValue(), 
	'... these should be the same reg-ex refs');	
	
# they should both be misc object refs
is(ref($clone->getChild(6)->getNodeValue()), "Misc", '... these should be misc object refs');
is(ref($tree->getChild(6)->getNodeValue()), "Misc", '... these should be misc object refs');
# and still the same
is($clone->getChild(6)->getNodeValue(), $tree->getChild(6)->getNodeValue(), 
	'... these should be the same misc object refs');	
	
# they should both be Tree::Simple objects
is(ref($clone->getChild(7)->getNodeValue()), "Tree::Simple", '... these should be Tree::Simple');
is(ref($tree->getChild(7)->getNodeValue()), "Tree::Simple", '... these should be Tree::Simple');
# but different ones
isnt($clone->getChild(7)->getNodeValue(), $tree->getChild(7)->getNodeValue(), 
	'... these should be different Tree::Simple objects');	
# with the same value	
is($clone->getChild(7)->getNodeValue()->getNodeValue(), $tree->getChild(7)->getNodeValue()->getNodeValue(), 
	'... these should have the same contents');	
    
# they should both be scalar refs
is(ref($clone->getChild(8)->getNodeValue()), "REF", '... these should be refs of refs');
is(ref($tree->getChild(8)->getNodeValue()), "REF", '... these should be refs of refs');
# but different ones
isnt($clone->getChild(8)->getNodeValue(), $tree->getChild(8)->getNodeValue(), 
	'... these should be different scalar refs');
# with the same ref value
is(${${$clone->getChild(8)->getNodeValue()}}, ${${$tree->getChild(8)->getNodeValue()}}, 
	'... these should be the same value');    

# test cloneShallow

my $shallow_clone = $tree->cloneShallow();

isnt($shallow_clone, $tree, '... these should be refs');

is_deeply(
		[ $shallow_clone->getAllChildren() ],
		[ $tree->getAllChildren() ],
		'... the children are the same');
		
my $sub_tree = $tree->getChild(7);
my $sub_tree_clone = $sub_tree->cloneShallow();
# but different ones
isnt($sub_tree_clone->getNodeValue(), $sub_tree->getNodeValue(), 
	'... these should be different Tree::Simple objects');		
# with the same value	
is($sub_tree_clone->getNodeValue()->getNodeValue(), $sub_tree->getNodeValue()->getNodeValue(), 
	'... these should have the same contents');	

