#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 9;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::VisitorFactory');
}

can_ok("Tree::Simple::VisitorFactory", 'new');

my $vf = Tree::Simple::VisitorFactory->new();
isa_ok($vf, 'Tree::Simple::VisitorFactory');

# test instance method
{
    can_ok($vf, 'get');
    my $visitor = $vf->get("PathToRoot");
    isa_ok($visitor, 'Tree::Simple::Visitor::PathToRoot');
}

# test class method
{
    can_ok("Tree::Simple::VisitorFactory", 'getVisitor');
    my $visitor = Tree::Simple::VisitorFactory->getVisitor("FindByPath");
    isa_ok($visitor, 'Tree::Simple::Visitor::FindByPath');
}

# test a few error conditions

throws_ok { 
    Tree::Simple::VisitorFactory->get();
} qr/Insufficient Arguments/, '... this should die';

throws_ok {
    $vf->getVisitor("ThisVisitorDoesNotExist");
} qr/Illegal Operation/, '... this should die';