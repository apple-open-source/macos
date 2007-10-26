
package Tree::Simple::Visitor::PathToRoot;

use strict;
use warnings;

our $VERSION = '0.02';

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

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object";
    # create an array for our path
    my @path;
    # we need to climb up the tree and 
    # collect the nodes
    my $filter_function = $self->getNodeFilter();
    my $current_tree = $tree;
    until ($current_tree->isRoot()) {  
        unshift @path => ($filter_function ?
                                        $filter_function->($current_tree)
                                        :
                                        $current_tree->getNodeValue());
        $current_tree = $current_tree->getParent();
    }
    # now grab the trunk if specified
    unshift @path => ($filter_function ?
                                        $filter_function->($current_tree)
                                        :
                                        $current_tree->getNodeValue()) if $self->includeTrunk();
    # now store our path in results
    $self->setResults(@path);                                    
}

sub getPath {
    my ($self) = @_;
    return $self->getResults();
}

sub getPathAsString {
    my ($self, $delimiter) = @_;
    $delimiter ||= ", ";
    return join $delimiter => $self->getResults();
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::PathToRoot - A Visitor for finding the path back a Tree::Simple object's root

=head1 SYNOPSIS

  use Tree::Simple::Visitor::PathToRoot;
  
  # create an instance of our visitor
  my $visitor = Tree::Simple::Visitor::PathToRoot->new();
  
  # pass the visitor to a Tree::Simple object
  $tree->accept($visitor);
  
  # now get the accumulated path as a string 
  # with the '/' character as the delimiter
  print $visitor->getPathAsString("/");
  
  # include the tree's trunk in your
  # output as well 
  $visitor->includeTrunk();
  
  # for more complex node objects, you can specify 
  # a node filter which will be used to extract the
  # information desired from each node
  $visitor->setNodeFilter(sub { 
                my ($t) = @_;
                return $t->getNodeValue()->description();
                });
  
  # you can also get the path back as an array
  my @path = $visitor->getPath();  

=head1 DESCRIPTION

Given a Tree::Simple object, this Visitor will find the path back to the tree's root node. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<includeTrunk> and C<setNodeFilter> methods to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to collect the trunk of the tree as well. 

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are collected. This can be used to customize output, or to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getPath>

This will return the collected path as an array, or in scalar context, as an array reference.

=item B<getPathAsString ($delimiter)>

This will return the collected path as a string with the path elements joined by a C<$delimiter>. If no C<$delimiter> is specified, the default (', ') will be used.

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

