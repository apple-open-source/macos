#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 32;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::LoadDirectoryTree');
    use_ok('Tree::Simple::Visitor::GetAllDescendents');
}

use Tree::Simple;
use File::Spec;

can_ok("Tree::Simple::Visitor::LoadDirectoryTree", 'new');

my @normal = qw(
    Changes
    lib
        Tree
            Simple
                Visitor
                    BreadthFirstTraversal.pm
                    CreateDirectoryTree.pm
                    FindByPath.pm
                    FindByUID.pm
                    FindByNodeValue.pm
                    FromNestedArray.pm                        
                    FromNestedHash.pm                  
                    GetAllDescendents.pm
                    LoadClassHierarchy.pm
                    LoadDirectoryTree.pm
                    PathToRoot.pm
                    PostOrderTraversal.pm 
                    PreOrderTraversal.pm 
                    Sort.pm 
                    ToNestedArray.pm
                    ToNestedHash.pm
                    VariableDepthClone.pm
                VisitorFactory.pm   
    Makefile.PL
    MANIFEST
    README
    t
        10_Tree_Simple_VisitorFactory_test.t
    	20_Tree_Simple_Visitor_PathToRoot_test.t
    	30_Tree_Simple_Visitor_FindByPath_test.t
        32_Tree_Simple_Visitor_FindByNodeValue_test.t
        35_Tree_Simple_Visitor_FindByUID_test.t
    	40_Tree_Simple_Visitor_GetAllDescendents_test.t
    	50_Tree_Simple_Visitor_BreadthFirstTraversal_test.t
    	60_Tree_Simple_Visitor_PostOrderTraversal_test.t
        65_Tree_Simple_Visitor_PreOrederTraversal_test.t
    	70_Tree_Simple_Visitor_LoadDirectoryTree_test.t
        75_Tree_Simple_Visitor_CreateDirectoryTree_test.t
        80_Tree_Simple_Visitor_Sort_test.t
        90_Tree_Simple_Visitor_FromNestedHash_test.t
        91_Tree_Simple_Visitor_FromNestedArray_test.t
        92_Tree_Simple_Visitor_ToNestedHash_test.t 
        93_Tree_Simple_Visitor_ToNestedArray_test.t  
        95_Tree_Simple_Visitor_LoadClassHierarchy_test.t 
        96_Tree_Simple_Visitor_VariableDepthClone_test.t       
    	pod.t
    	pod_coverage.t
);
my %normal = map { $_ => undef } @normal;

my $node_filter = sub {
    my ($item) = @_;
    return 0 unless exists $normal{$item};
    return 1;
};

# normal order
{
    my $dir_tree = Tree::Simple->new(File::Spec->curdir(), Tree::Simple->ROOT);
    isa_ok($dir_tree, 'Tree::Simple');

    my $visitor = Tree::Simple::Visitor::LoadDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    # just examine the files in the MANIFEST
    # not the ones created by the makefile
    $visitor->setNodeFilter($node_filter);
    
    $dir_tree->accept($visitor);

    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');
    
    $dir_tree->accept($visitor_check);
    
    # we have to sort these because different OSes
    # will return the results in different orders.
    is_deeply(
            [ sort $visitor_check->getResults() ],
            [ sort @normal ],
            '... our tree is in the proper order'); 
}

# file first order
{
    my $dir_tree = Tree::Simple->new(File::Spec->curdir(), Tree::Simple->ROOT);
    isa_ok($dir_tree, 'Tree::Simple');

    my $visitor = Tree::Simple::Visitor::LoadDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    # just examine the files in the MANIFEST
    # not the ones created by the makefile 
    $visitor->setNodeFilter($node_filter);
    
    can_ok($visitor, 'SORT_FILES_FIRST');
    $visitor->setSortStyle($visitor->SORT_FILES_FIRST);
    
    $dir_tree->accept($visitor);
    
    my @files_first = qw(
        Changes
        Makefile.PL
        MANIFEST
        README        
        lib
            Tree
                Simple
                    VisitorFactory.pm                  
                    Visitor
                        BreadthFirstTraversal.pm
                        CreateDirectoryTree.pm
                        FindByNodeValue.pm
                        FindByPath.pm                        
                        FindByUID.pm
                        FromNestedArray.pm                        
                        FromNestedHash.pm                      
                        GetAllDescendents.pm
                        LoadClassHierarchy.pm                        
                        LoadDirectoryTree.pm
                        PathToRoot.pm
                        PostOrderTraversal.pm    
                        PreOrderTraversal.pm  
                        Sort.pm  
                        ToNestedArray.pm
                        ToNestedHash.pm 
                        VariableDepthClone.pm                                           
        t
            10_Tree_Simple_VisitorFactory_test.t
        	20_Tree_Simple_Visitor_PathToRoot_test.t
        	30_Tree_Simple_Visitor_FindByPath_test.t
            32_Tree_Simple_Visitor_FindByNodeValue_test.t
            35_Tree_Simple_Visitor_FindByUID_test.t
        	40_Tree_Simple_Visitor_GetAllDescendents_test.t
        	50_Tree_Simple_Visitor_BreadthFirstTraversal_test.t
        	60_Tree_Simple_Visitor_PostOrderTraversal_test.t
            65_Tree_Simple_Visitor_PreOrederTraversal_test.t
        	70_Tree_Simple_Visitor_LoadDirectoryTree_test.t
            75_Tree_Simple_Visitor_CreateDirectoryTree_test.t
            80_Tree_Simple_Visitor_Sort_test.t
            90_Tree_Simple_Visitor_FromNestedHash_test.t
            91_Tree_Simple_Visitor_FromNestedArray_test.t 
            92_Tree_Simple_Visitor_ToNestedHash_test.t 
            93_Tree_Simple_Visitor_ToNestedArray_test.t  
            95_Tree_Simple_Visitor_LoadClassHierarchy_test.t        
            96_Tree_Simple_Visitor_VariableDepthClone_test.t 
        	pod.t
        	pod_coverage.t
    );    

    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');
    
    $dir_tree->accept($visitor_check);
        
    is_deeply(
            [ $visitor_check->getResults() ],
            \@files_first,            
            '... our tree is in the file first order'); 
}


# dir first order
{
    my $dir_tree = Tree::Simple->new(File::Spec->curdir(), Tree::Simple->ROOT);
    isa_ok($dir_tree, 'Tree::Simple');

    my $visitor = Tree::Simple::Visitor::LoadDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    # just examine the files in the MANIFEST
    # not the ones created by the makefile   
    $visitor->setNodeFilter($node_filter);
    
    can_ok($visitor, 'SORT_DIRS_FIRST');
    $visitor->setSortStyle($visitor->SORT_DIRS_FIRST);
    
    $dir_tree->accept($visitor);
    
    my @dirs_first = qw(       
        lib
            Tree
                Simple
                    Visitor
                        BreadthFirstTraversal.pm
                        CreateDirectoryTree.pm
                        FindByNodeValue.pm                        
                        FindByPath.pm
                        FindByUID.pm
                        FromNestedArray.pm                        
                        FromNestedHash.pm                        
                        GetAllDescendents.pm
                        LoadClassHierarchy.pm                        
                        LoadDirectoryTree.pm
                        PathToRoot.pm
                        PostOrderTraversal.pm   
                        PreOrderTraversal.pm 
                        Sort.pm        
                        ToNestedArray.pm
                        ToNestedHash.pm   
                        VariableDepthClone.pm                                    
                    VisitorFactory.pm   
        t
            10_Tree_Simple_VisitorFactory_test.t
        	20_Tree_Simple_Visitor_PathToRoot_test.t
        	30_Tree_Simple_Visitor_FindByPath_test.t
            32_Tree_Simple_Visitor_FindByNodeValue_test.t
            35_Tree_Simple_Visitor_FindByUID_test.t
        	40_Tree_Simple_Visitor_GetAllDescendents_test.t
        	50_Tree_Simple_Visitor_BreadthFirstTraversal_test.t
        	60_Tree_Simple_Visitor_PostOrderTraversal_test.t
            65_Tree_Simple_Visitor_PreOrederTraversal_test.t
        	70_Tree_Simple_Visitor_LoadDirectoryTree_test.t
            75_Tree_Simple_Visitor_CreateDirectoryTree_test.t
            80_Tree_Simple_Visitor_Sort_test.t
            90_Tree_Simple_Visitor_FromNestedHash_test.t
            91_Tree_Simple_Visitor_FromNestedArray_test.t   
            92_Tree_Simple_Visitor_ToNestedHash_test.t 
            93_Tree_Simple_Visitor_ToNestedArray_test.t   
            95_Tree_Simple_Visitor_LoadClassHierarchy_test.t     
            96_Tree_Simple_Visitor_VariableDepthClone_test.t               
        	pod.t
        	pod_coverage.t  
        Changes
        Makefile.PL
        MANIFEST
        README                             
    );

    my $visitor_check = Tree::Simple::Visitor::GetAllDescendents->new();
    isa_ok($visitor_check, 'Tree::Simple::Visitor::GetAllDescendents');
    
    $dir_tree->accept($visitor_check);
        
    is_deeply(
            [ $visitor_check->getResults() ],
            \@dirs_first,
            '... our tree is in the dir first order'); 
}

# test the errors
{
    my $visitor = Tree::Simple::Visitor::LoadDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::LoadDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    # check setSortStyle
    can_ok($visitor, 'setSortStyle');
    throws_ok {
        $visitor->setSortStyle();
    } qr/Insufficient Arguments/, '... got the error we expected';
    
    throws_ok {
        $visitor->setSortStyle("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';    
    
    throws_ok {
        $visitor->setSortStyle([]);
    } qr/Insufficient Arguments/, '... got the error we expected'; 
    
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
    
    # check that tree is a leaf
    
    my $tree = Tree::Simple->new("test")->addChild(Tree::Simple->new("test 2"));
    
    throws_ok {
        $visitor->visit($tree);
    } qr/Illegal Operation/, '... got the error we expected';    
    
    throws_ok {
        $visitor->visit($tree->getChild(0));
    } qr/Incorrect Type/, '... got the error we expected';        
                
}
