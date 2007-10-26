#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 36;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::VariableDepthClone');
}

use Tree::Simple;
use Tree::Simple::Visitor::PreOrderTraversal;

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.1"),
                                            Tree::Simple->new("1.2")
                                                        ->addChildren(
                                                            Tree::Simple->new("1.2.1"),
                                                            Tree::Simple->new("1.2.2")
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
                                            Tree::Simple->new("3.2"),
                                            Tree::Simple->new("3.3")                                                                                                
                                        ),                            
                            Tree::Simple->new("4")                                                        
                                        ->addChildren(
                                            Tree::Simple->new("4.1")
                                        )                            
                       );
isa_ok($tree, 'Tree::Simple');

can_ok("Tree::Simple::Visitor::VariableDepthClone", 'new');

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');
    isa_ok($visitor, 'Tree::Simple::Visitor');

    can_ok($visitor, 'setCloneDepth');
    can_ok($visitor, 'getClone');

    $visitor->setCloneDepth(2);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1 1.1 1.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(1);
    $visitor->setNodeFilter(sub {
        my ($old, $new) = @_;
        $new->setNodeValue($old->getNodeValue() . "new");
    });
    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1new 2new 3new 4new) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(3);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1 1.1 1.2 1.2.1 1.2.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(100);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1 1.1 1.2 1.2.1 1.2.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(0);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(-1);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->setCloneDepth(-100);

    $tree->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [],
        '... our results are as expected');
}

# check with trunk

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->includeTrunk(1);
    $visitor->setCloneDepth(2);
    $visitor->setNodeFilter(sub {
        my ($old, $new) = @_;
        $new->setNodeValue($old->getNodeValue() . "new");
    });    

    $tree->getChild(0)->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1new 1.1new 1.2new 1.2.1new 1.2.2new 1.3new) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->includeTrunk(1);
    $visitor->setCloneDepth(1);

    $tree->getChild(0)->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1 1.1 1.2 1.3) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->includeTrunk(1);
    $visitor->setCloneDepth(0);

    $tree->getChild(0)->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->includeTrunk(1);
    $visitor->setCloneDepth(-1);

    $tree->getChild(0)->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1) ],
        '... our results are as expected');
}

{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

    $visitor->includeTrunk(1);
    $visitor->setCloneDepth(-100);

    $tree->getChild(0)->getChild(0)->accept($visitor);

    my $cloned = $visitor->getClone();

    my $checker = Tree::Simple::Visitor::PreOrderTraversal->new();
    $cloned->accept($checker);
    is_deeply(
        [ $checker->getResults() ],
        [ qw(1.1) ],
        '... our results are as expected');
}

# check some errors

# check errors
{
    my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::VariableDepthClone');

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

    throws_ok {
        $visitor->setCloneDepth();
    } qr/Insufficient Arguments/, '... got the error we expected';                      

}
