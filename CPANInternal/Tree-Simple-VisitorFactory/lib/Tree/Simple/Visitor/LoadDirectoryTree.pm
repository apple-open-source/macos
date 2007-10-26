
package Tree::Simple::Visitor::LoadDirectoryTree;

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
    $self->{sort_function} = undef;
    $self->SUPER::_init();    
}

# pre-built sort functions
sub SORT_FILES_FIRST {
    return sub ($$$) { 
        my ($path, $left, $right) = @_;
        $left  = File::Spec->catdir($path, $left);
        $right = File::Spec->catdir($path, $right);    
        return ((-d $left && -f $right) ? 1 :       # file beats directory
                (-d $right && -f $left) ? -1 :    # file beats directory
                    (lc($left) cmp lc($right)))     # otherwise just sort 'em
    }
}

sub SORT_DIRS_FIRST {
    return sub ($$$) {  
        my ($path, $left, $right) = @_;
        $left  = File::Spec->catdir($path, $left);
        $right = File::Spec->catdir($path, $right);   
        return ((-d $left && -f $right) ? -1 :      # directory beats file
                (-d $right && -f $left) ? 1 :     # directory beats file
                    (lc($left) cmp lc($right)))     # otherwise just sort 'em
    }
}

sub setSortStyle {
    my ($self, $sort_function) = @_;
	(defined($sort_function) && ref($sort_function) eq "CODE") 
		|| die "Insufficient Arguments : sort function argument must be a subroutine reference";    
    $self->{sort_function} = $sort_function;
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
    # it must be a leaf
    ($tree->isLeaf()) || die "Illegal Operation : The tree must be a leaf node to load a directory";
    # check that our directory is valid
    my $root_dir = $tree->getNodeValue();
    (-e $root_dir && -d $root_dir) 
        || die "Incorrect Type : The tree's node value must be a valid directory";  
    # and load it recursively
    $self->_recursiveLoad($tree, $root_dir);
}

sub _recursiveLoad {
	my ($self, $t, $path) = @_; 
    # get a node filter if we have one
    my $filter = $self->getNodeFilter();
    
    # get the contents of the directory
    opendir(DIR, $path) || die "IO Error : Could not open directory : $!";
    # avoid the . and .. symbolic links
    my @dir_contents = grep { 
                        $_ ne File::Spec->curdir() && $_ ne File::Spec->updir()
                        } readdir(DIR);
    close(DIR);
    
    # sort them if we need to with full paths
    @dir_contents = sort { 
                        $self->{sort_function}->($path, $a, $b) 
                    } @dir_contents if $self->{sort_function};

    # now traverse ...
	foreach my $item (@dir_contents) {
        # filter based on the item name
        $filter->($item) || next if defined($filter);
        # get the full path for checking
        # the item type and recursion
        my $full_path = File::Spec->catdir($path, $item);
		if (-d $full_path) {
            my $new_tree = $t->new($item);
            $t->addChild($new_tree);       
            $self->_recursiveLoad($new_tree, $full_path);
		}
		elsif (-f $full_path) {
            $t->addChild($t->new($item));
		}
	}
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::LoadDirectoryTree - A Visitor for loading the contents of a directory into a Tree::Simple object

=head1 SYNOPSIS

  use Tree::Simple::Visitor::LoadDirectoryTree;
  
  # create a Tree::Simple object whose
  # node is path to a directory
  my $tree = Tree::Simple->new("./");

  # create an instance of our visitor
  my $visitor = Tree::Simple::Visitor::LoadDirectoryTree->new();
  
  # set the directory sorting style
  $visitor->setSortStyle($visitor->SORT_FILES_FIRST);
  
  # create node filter to filter 
  # out certain files and directories
  $visitor->setNodeFilter(sub {
      my ($item) = @_;
      return 0 if $item =~ /CVS/;
      return 1;
  });  
  
  # pass the visitor to a Tree::Simple object
  $tree->accept($visitor);
  
  # the tree now mirrors the structure of the directory 

=head1 DESCRIPTION

This visitor can be used to load a directory tree into a Tree::Simple hierarchy.

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> and C<setSortStyle> methods to customize its behavior.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are created. The function is given the current directory or file being added to the tree, and it is expected to return either true (C<1>) of false (C<0>) to determine if that directory should be traversed or file added to the tree.

=item B<setSortStyle ($sort_function)>

This method accepts a CODE reference as its C<$sort_function> argument and throws an exception if it is not a code reference. This function is used to sort the individual levels of the directory tree right before it is added to the tree being built. The function is passed the the current path, followed by the two items being sorted. The reason for passing the path in is so that sorting operations can be performed on the entire path if desired. 

Two pre-built functions are supplied and described below. 

=over 4

=item B<SORT_FILES_FIRST>

This sorting function will sort files before directories, so that files are sorted alphabetically first in the list followed by directories sorted alphabetically. Here is example of how that would look:

    Tree/
        Simple.pm
        Simple/
            Visitor.pm
            VisitorFactory.pm
            Visitor/
                PathToRoot.pm

=item B<SORT_DIRS_FIRST>

This sorting function will sort directories before files, so that directories are sorted alphabetically first in the list followed by files sorted alphabetically. Here is example of how that would look:

    Tree/
        Simple/
            Visitor/
                PathToRoot.pm
            Visitor.pm
            VisitorFactory.pm
        Simple.pm

=back

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

The node value of the C<$tree> argument (gotten by calling C<getNodeValue>) is considered the root directory from which we begin our traversal. We use File::Spec to keep our paths cross-platform, but it is expected that you will feed in a valid path for your OS. If the path either does not exist, or is not a directory, then an exception is thrown.

The C<$tree> argument which is passed to C<visit> must be a leaf node. This is because this Visitor will create all the sub-nodes for this tree. If the tree is not a leaf, an exception is thrown. We do not require the tree to be a root though, and this Visitor will not affect any nodes above the C<$tree> argument.

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

