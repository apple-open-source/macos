#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 57;
use Test::Exception;

BEGIN { 
    use_ok('Tree::Simple::Visitor::CreateDirectoryTree');
}

use Tree::Simple;
use File::Spec;

my $dir_tree = Tree::Simple->new("test/")
                    ->addChildren(
                            Tree::Simple->new("sub_test/"),
                            Tree::Simple->new("test.pm"),
                            Tree::Simple->new("sub_test2\\")
                                ->addChildren(
                                    Tree::Simple->new("sub_sub_test/"),
                                    Tree::Simple->new("sub_test.pm"),
                                    Tree::Simple->new("sub_sub_sub_test\\")
                                        ->addChildren(
                                            Tree::Simple->new("sub_sub_sub_test.pm")
                                        )
                                )
                    );
isa_ok($dir_tree, 'Tree::Simple');                    

can_ok("Tree::Simple::Visitor::CreateDirectoryTree", 'new');

# test the default behavior
{
    my $visitor = Tree::Simple::Visitor::CreateDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::CreateDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'visit');
    
    $dir_tree->accept($visitor);
    
    # these are all the files we created
    my @files = (
        File::Spec->catfile("test", "test.pm"),
        File::Spec->catfile("test", "sub_test2", "sub_test.pm"),
        File::Spec->catfile("test", "sub_test2", "sub_sub_sub_test", "sub_sub_sub_test.pm")
        );
        
    # loop through and check them 
    # and then remove them   
    foreach my $filename (@files) {
        ok(-e $filename, '... file exists');
        ok(-f $filename, '... and it is a file');
        # now remove it
        cmp_ok(unlink($filename), '==', 1, '... removed file');
        ok(!-e $filename, '... file is actually gone');    
    }    
    
    # these are all the directories 
    # we created (in reverse order)
    my @directories = reverse(
        "test",
        File::Spec->catdir("test", "sub_test"),
        File::Spec->catdir("test", "sub_test2"),
        File::Spec->catdir("test", "sub_test2", "sub_sub_test"),
        File::Spec->catdir("test", "sub_test2", "sub_sub_sub_test")
        );    
    
    # loop through and check them    
    # and remove them (reverse order
    # insures that they are empty when
    # we remove them)
    foreach my $dirname (@directories) {
        ok(-e $dirname, '... directory exists');
        ok(-d $dirname, '... and it is a directory');
        # now remove it
        cmp_ok(rmdir($dirname), '==', 1, '... removed directory');
        ok(!-e $dirname, '... directory is actually gone');    
    }   
}

# test the file and dir handlers
{
    my $visitor = Tree::Simple::Visitor::CreateDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::CreateDirectoryTree');
    isa_ok($visitor, 'Tree::Simple::Visitor');
    
    can_ok($visitor, 'visit');
    can_ok($visitor, 'setFileHandler');
    can_ok($visitor, 'setDirectoryHandler');    
    
    $visitor->setNodeFilter(sub {
        my ($node) = @_;
        return "_$node";
    });
    
    # capture the directories
    # in an array, but don't bother 
    # to create anything
    my @dirs;
    $visitor->setDirectoryHandler(sub {
        my ($dir_path) = @_;
        push @dirs => $dir_path;
    });

    # these are the expected values
    my @expected_dirs = (
        "_test",
        File::Spec->catdir("_test", "_sub_test"),
        File::Spec->catdir("_test", "_sub_test2"),
        File::Spec->catdir("_test", "_sub_test2", "_sub_sub_test"),
        File::Spec->catdir("_test", "_sub_test2", "_sub_sub_sub_test")
        );   

    # capture the files
    # in an array, but don't bother 
    # to create anything    
    my @files;
    $visitor->setFileHandler(sub {
        my ($file_path) = @_;
        push @files => $file_path;
    });
    
    # these are the expected values
    my @expected_files = (
        File::Spec->catfile("_test", "_test.pm"),
        File::Spec->catfile("_test", "_sub_test2", "_sub_test.pm"),
        File::Spec->catfile("_test", "_sub_test2", "_sub_sub_sub_test", "_sub_sub_sub_test.pm")
        );    
    
    $dir_tree->accept($visitor);
    
    is_deeply(\@dirs, \@expected_dirs, '... got the directories we expected');
    is_deeply(\@files, \@expected_files, '... got the files we expected');
}


# test the errors
{
    my $visitor = Tree::Simple::Visitor::CreateDirectoryTree->new();
    isa_ok($visitor, 'Tree::Simple::Visitor::CreateDirectoryTree');
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
        $visitor->setDirectoryHandler();
    } qr/Insufficient Arguments/, '... got the error we expected';      
    
    throws_ok {
        $visitor->setDirectoryHandler("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';     
    
    throws_ok {
        $visitor->setDirectoryHandler([]);
    } qr/Insufficient Arguments/, '... got the error we expected';    
    
    # check the handler errors
    throws_ok {
        $visitor->setFileHandler();
    } qr/Insufficient Arguments/, '... got the error we expected';      
    
    throws_ok {
        $visitor->setFileHandler("Fail");
    } qr/Insufficient Arguments/, '... got the error we expected';     
    
    throws_ok {
        $visitor->setFileHandler([]);
    } qr/Insufficient Arguments/, '... got the error we expected';        
}
