
package Tree::Simple::Visitor::CreateDirectoryTree;

use strict;
use warnings;

our $VERSION = '0.02';

use File::Spec;
use Scalar::Util qw(blessed);

use base qw(Tree::Simple::Visitor);

sub new {
    my ($_class) = @_;
    my $class = ref($_class) || $_class;
    my $visitor = {};
    bless($visitor, $class);
    $visitor->_init();
    return $visitor;
}

sub _init {
    my ($self) = @_;
    $self->{file_handler} = sub {        
        my ($filepath) = @_;
        open(FILE, ">", $filepath) || die "IO Error : Cannot create file ($filepath) : $!";
        close(FILE);    
    };
    $self->{dir_handler} = sub {
        my ($dirpath) = @_;
        mkdir($dirpath) || die "IO Error : Cannot make directory ($dirpath) : $!";
    };
    $self->SUPER::_init();    
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
    # pass on to our recursive subroutine
    $self->_createDirectoryStructure($tree);
}

sub setFileHandler {
    my ($self, $file_handler) = @_;
    (defined($file_handler) && ref($file_handler) eq 'CODE')
        || die "Insufficient Arguments : file handler must be a subroutine reference";
    $self->{file_handler} = $file_handler;
}

sub setDirectoryHandler {
    my ($self, $dir_handler) = @_;
    (defined($dir_handler) && ref($dir_handler) eq 'CODE')
        || die "Insufficient Arguments : directory handler must be a subroutine reference";
    $self->{dir_handler} = $dir_handler;    
}

sub _createDirectoryStructure {
    my ($self, $tree, @path) = @_;
    my $node = $tree->getNodeValue();
    # filter the nodes if need be
    my $filter_function = $self->getNodeFilter();
    $node = $filter_function->($node) if $filter_function;
    # if its a leaf and it 
    # doesn't end with a /
    # then its a file
    if ($tree->isLeaf() && $node !~ /\/|\\$/) {
        $self->{file_handler}->(File::Spec->catfile(@path, $node));
    }	    
    # otherwise we are going 
    # to treat it as a directory
    else {
        $node =~ s/\/|\\$//;
        $self->{dir_handler}->(File::Spec->catdir(@path, $node));
        foreach my $child ($tree->getAllChildren()) {
            $self->_createDirectoryStructure($child, (@path, $node));
        }
    }
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::CreateDirectoryTree - A Visitor for create a set of directories and files from a Tree::Simple object

=head1 SYNOPSIS

  use Tree::Simple::Visitor::CreateDirectoryTree;
  
  # create a Tree::Simple object which
  # represents a directory heirarchy
  my $tree = Tree::Simple->new("www/")
                    ->addChildren(
                        Tree::Simple->new("conf/")
                            ->addChildren(
                                Tree::Simple->new("startup.pl"),
                                Tree::Simple->new("httpd.conf")
                            ),                            
                        Tree::Simple->new("cgi-bin/"),
                        Tree::Simple->new("ht_docs/"),
                        Tree::Simple->new("logs/")
                            ->addChildren(
                                Tree::Simple->new("error.log"),
                                Tree::Simple->new("access.log")
                            ),                            
                    );

  # create an instance of our visitor
  my $visitor = Tree::Simple::Visitor::CreateDirectoryTree->new();
  
  # pass the visitor to a Tree::Simple object
  $tree->accept($visitor);
  
  # the www/ directory now mirrors the structure of the tree 

=head1 DESCRIPTION

This visitor can be used to create a set of directories and files from a Tree::Simple object hierarchy.

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter>, C<setFileHandler> and C<setDirectoryHandler> methods to customize its behavior.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are used to create the directory tree, it can be basically used as a node pre-processor. An example usage of this might be to enforce the C<8.3> naming rules of DOS, or the 32 character limit of older macintoshes.   

=item B<setFileHandler ($file_handler)>

This method accepts a CODE reference as its C<$file_handler> argument and throws an exception if it is not a CODE reference. This method can be used to create custom file creation behavior. The default behavior is to just create the file and nothing else, but by using this method it is possible to implement some other custom behavior, such as creating a file based on a template. The function is passed the full path of the file to be created (as built by File::Spec).

=item B<setDirectoryHandler ($dir_handler)>

This method accepts a CODE reference as its C<$dir_handler> argument and throws an exception if it is not a CODE reference. This method can be used to create custom directory creation behavior. The default behavior is to just create the directory and nothing else, but by using this method it is possible to implement some other custom behavior, such as creating a directory on a remote server. The function is passed the full path of the directory to be created (as built by File::Spec).

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

The tree is processed as follows:

=over 4

=item Any node which is not a leaf is considered a directory.

Obviously since files themselves are leaf nodes, this makes sense that non-leaves will be directories.

=item Any node (including leaf nodes) which ends in either the character C</> or C<\> is considered a directory.

I think it is a pretty standard convention to have directory names ending in a seperator. The seperator itself is stripped off before the directory name is passed to File::Spec where the platform specific directory path is created. This means that it does not matter which one you use, it will be completely cross platform (at least as cross-platform as File::Spec is).

=item All other nodes are considered to be files.

=back

=back

=head1 BUGS

None that I am aware of. Of course, if you find a bug, let me know, and I will be sure to fix it. 

=head1 CODE COVERAGE

See the B<CODE COVERAGE> section in L<Tree::Simple::VisitorFactory> for more inforamtion.

=head1 SEE ALSO

These Visitor classes are all subclasses of B<Tree::Simple::Visitor>, which can be found in the B<Tree::Simple> module, you should refer to that module for more information.

=head1 AUTHOR

stevan little, E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004, 2005 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

