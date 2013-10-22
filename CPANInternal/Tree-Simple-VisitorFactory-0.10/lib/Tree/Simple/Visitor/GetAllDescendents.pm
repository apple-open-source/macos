
package Tree::Simple::Visitor::GetAllDescendents;

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
    $self->{traversal_method} = undef;
    $self->SUPER::_init();    
}

sub setTraversalMethod {
	my ($self, $visitor) = @_;
	(blessed($visitor) && $visitor->isa("Tree::Simple::Visitor"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple::Visitor object";
    $self->{traversal_method} = $visitor;
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object";
	# create an closure for the 
    # collection function
    my @descendents;
    my $filter_function = $self->getNodeFilter();
    # build a collection function
    my $collection_function = sub {
        my ($t) = @_;
        push @descendents => ($filter_function ?
                                $filter_function->($t)
                                :
                                $t->getNodeValue());
    };
    # and collect our descendents with the 
    # traversal method specified   
    unless (defined($self->{traversal_method})) {
        $tree->traverse($collection_function);
    }
    else {
        $self->{traversal_method}->setNodeFilter($collection_function);
        $self->{traversal_method}->visit($tree);
    }
    # now store our collected descendents
    $self->setResults(@descendents);
}

sub getAllDescendents {
    my ($self) = @_;
    return $self->getResults(); 
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::GetAllDescendents - A Visitor for fetching all the descendents of a Tree::Simple object

=head1 SYNOPSIS

  use Tree::Simple::Visitor::GetAllDescendents;

  # create an instance of our visitor
  my $visitor = Tree::Simple::Visitor::GetAllDescendents->new();
  
  # pass the visitor to a Tree::Simple object
  $tree->accept($visitor);

  # you can also get the descendents 
  # back as an array of node values
  my @descendents = $visitor->getDescendents(); 
  
  # for more complex node objects, you can specify 
  # a node filter which will be used to extract the
  # information desired from each node
  $visitor->setNodeFilter(sub { 
                my ($t) = @_;
                return $t->getNodeValue()->description();
                });

=head1 DESCRIPTION

Given a Tree::Simple instance this Visitor will return all the descendents recursively on down the hierarchy.

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> method to customize its behavior.

=item B<setTraversalMethod ($visitor)>

By default we will use Tree::Simple's built in depth-first (pre-order) traverse method. If however, you desire the descendents to be returned in a different ordering, this can be accomplished using a different traversal method, you can supply a C<$visitor> object implementing that traversal type to this method (See  B<Tree::Simple::Visitor::BreadthFirstTraversal>, B<Tree::Simple::Visitor::PreOrderTraversal> and B<Tree::Simple::Visitor::PostOrderTraversal>).

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are collected. This can be used to customize output, or to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getAllDescendents>

This method will give back and array of descendents in depth-first order (pre-order) or in the order specified by the C<setTraversalMethod>. If called in scalar context it will give an array reference, in list context it will return a regular array. This method is the same as calling C<getResults>.

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

