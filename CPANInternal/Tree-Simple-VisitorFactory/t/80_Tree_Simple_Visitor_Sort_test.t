#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 37;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::Sort');
    use_ok('Tree::Simple::Visitor::GetAllDescendents');
}

use Tree::Simple;

my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new("1")
                                        ->addChildren(
                                            Tree::Simple->new("1.3"),
                                            Tree::Simple->new("1.2")
                                                        ->addChildren(
                                                            Tree::Simple->new("1.2.2"),
                                                            Tree::Simple->new("1.2.1")
                                                        ),
                                            Tree::Simple->new("1.1")                                                                                                
                                        ),
                            Tree::Simple->new("4")                                                        
                                        ->addChildren(
                                            Tree::Simple->new("4.1")
                                        ),    
                            Tree::Simple->new("2")
                                        ->addChildren(
                                            Tree::Simple->new("2.1"),
                                            Tree::Simple->new("2.2")
                                        ),                            
                            Tree::Simple->new("3")
                                        ->addChildren(
                                            Tree::Simple->new("3.3"),
                                            Tree::Simple->new("3.2"),
                                            Tree::Simple->new("3.1")                                                                                                
                                        )                            
                        
                       );
isa_ok($tree, 'Tree::Simple');

can_ok("Tree::Simple::Visitor::Sort", 'new');

# try normal sort
{    
    my $visitor = Tree::Simple::Visitor::Sort->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::Sort');
    isa_ok($visitor, 'Tree::Simple::Visitor');    
    
    can_ok($visitor, 'visit');
    
    $tree->accept($visitor);
    
    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');

    $tree->accept($visitor_check);    
    
    is_deeply(
        [ $visitor_check->getAllDescendents() ], 
        [ qw/1 1.1 1.2 1.2.1 1.2.2 1.3 2 2.1 2.2 3 3.1 3.2 3.3 4 4.1/ ], 
        '... our tree is as expected after sort');
    
}

# try sort with a node filter 
{    
    my $visitor = Tree::Simple::Visitor::Sort->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::Sort');
    
    my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                       ->addChildren(
                            Tree::Simple->new([ 1 ])
                                        ->addChildren(
                                            Tree::Simple->new([ 1, 3 ]),
                                            Tree::Simple->new([ 1, 2 ])
                                                        ->addChildren(
                                                            Tree::Simple->new([ 1, 2, 2 ]),
                                                            Tree::Simple->new([ 1, 2, 1 ])
                                                        ),
                                            Tree::Simple->new([ 1, 1])                                                                                                
                                        ),  
                            Tree::Simple->new([ 2 ])
                                        ->addChildren(
                                            Tree::Simple->new([ 2, 1 ]),
                                            Tree::Simple->new([ 2, 2 ] )
                                        )                                                   
                       );
    isa_ok($tree, 'Tree::Simple');    
    
    can_ok($visitor, 'setNodeFilter');    
    $visitor->setNodeFilter(sub {
        my ($t) = @_;
        # sort on the last part of the node
        return $t->getNodeValue()->[-1];
    });              
    
    $tree->accept($visitor);
    
    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');

    $tree->accept($visitor_check);    
    
    is_deeply(
        [ $visitor_check->getAllDescendents() ], 
        [ [1], [1, 1], [1, 2], [1, 2, 1], [1, 2, 2], [1, 3], [2], [2, 1], [2, 2] ], 
        '... our tree is as expected after sort');
}
    
# try custom sort function 
{    
    my $visitor = Tree::Simple::Visitor::Sort->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::Sort');
    
    can_ok($visitor, 'setSortFunction');    
    $visitor->setSortFunction($visitor->REVERSE);    
    
    $tree->accept($visitor);
    
    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');

    $tree->accept($visitor_check);    
    
    is_deeply(
        [ $visitor_check->getAllDescendents() ], 
        [ qw/4 4.1 3 3.3 3.2 3.1 2 2.2 2.1 1 1.3 1.2 1.2.2 1.2.1 1.1 / ], 
        '... our tree is as expected after sort');
}

# check all our pre-built functions
is(ref(Tree::Simple::Visitor::Sort->REVERSE), 'CODE', '... it is a code reference');
# already tested above

is(ref(Tree::Simple::Visitor::Sort->NUMERIC), 'CODE', '... it is a code reference');
cmp_ok(Tree::Simple::Visitor::Sort->NUMERIC->(Tree::Simple->new(5), Tree::Simple->new(4)), 
        '==', 1, '... the numeric sort works');

is(ref(Tree::Simple::Visitor::Sort->REVERSE_NUMERIC),      'CODE', '... it is a code reference');
cmp_ok(Tree::Simple::Visitor::Sort->REVERSE_NUMERIC->(Tree::Simple->new(5), Tree::Simple->new(4)), 
        '==', -1, '... the reverse numeric sort works');

is(ref(Tree::Simple::Visitor::Sort->ALPHABETICAL),         'CODE', '... it is a code reference');
cmp_ok(Tree::Simple::Visitor::Sort->ALPHABETICAL->(Tree::Simple->new("A"), Tree::Simple->new("a")), 
        '==', 0, '... the alphabetical sort works');

is(ref(Tree::Simple::Visitor::Sort->REVERSE_ALPHABETICAL), 'CODE', '... it is a code reference');
cmp_ok(Tree::Simple::Visitor::Sort->REVERSE_ALPHABETICAL->(Tree::Simple->new("a"), Tree::Simple->new("b")), 
        '==', 1, '... the reverse alphabetical sort works');

# test some weird stuff
{

    my $visitor = Tree::Simple::Visitor::Sort->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::Sort'); 
    
    # test visitiing a leaf node
    my $leaf = Tree::Simple->new("leaf");
    $leaf->accept($visitor);

}

# test the errors
{
    my $visitor = Tree::Simple::Visitor::Sort->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::Sort');
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
    
    # check the handler errors
    throws_ok {
        $visitor->setSortFunction();
    } qr/Insufficient Arguments/, '... got the error we expected';      
    
    throws_ok {
        $visitor->setSortFunction("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';     
    
    throws_ok {
        $visitor->setSortFunction([]);
    } qr/Insufficient Arguments/, '... got the error we expected';    
          
}
