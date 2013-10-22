
package Tree::Simple::Visitor::BreadthFirstTraversal;

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

sub _init {
    my ($self) = @_;
    $self->SUPER::_init();
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object";
    # create a holder for our results
    my @results;
    # get our filter function
    my $filter_function = $self->getNodeFilter();
    # now create a queue for 
    # processing depth first
    my @queue;
    # if we are to include the trunk 
    if ($self->includeTrunk()) {
        # then enqueue that
        @queue = ($tree);
    }
    # if we are not including the trunk
    else {
        # then we enqueue all the 
        # trunks children instead
        @queue = ($tree->getAllChildren());
    }
    # until our queue is empty
    while (scalar(@queue) != 0){
        # get the first item off the queue
        my $current_tree = shift @queue;
        # enqueue all the current tree's children
        push @queue => $current_tree->getAllChildren();
        # now collect the results 
        push @results => (($filter_function) ? 
                                    $filter_function->($current_tree)
                                    :
                                    $current_tree->getNodeValue());
    }        
    # store our results
    $self->setResults(@results);    
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::BreadthFirstTraversal - A Visitor for breadth-first traversal a Tree::Simple hierarchy

=head1 SYNOPSIS

  use Tree::Simple::Visitor::BreadthFirstTraversal;
  
  # create an visitor
  my $visitor = Tree::Simple::Visitor::BreadthFirstTraversal->new();
  
  # pass our visitor to the tree
  $tree->accept($visitor);
  
  # print our results
  print join ", " => $visitor->getResults();
  
  # this will print this: 
  #   1, 2, 3, 1.1, 1.2, 2.1, 3.1, 1.1.1 
  # assuming your tree is like this:
  #   1
  #     1.1
  #       1.1.1
  #     1.2
  #   2
  #     2.1
  #   3
  #     3.1

=head1 DESCRIPTION

This implements a breadth-first traversal of a Tree::Simple hierarchy. This can be an alternative to the built in depth-first traversal of the Tree::Simple C<traverse> method.

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the  C<setNodeFilter> method to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to include the trunk of the tree in the traversal as well. 

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are collected. This can be used to customize output, or to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getResults>

This method returns the accumulated results of the application of the node filter to the tree.

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

