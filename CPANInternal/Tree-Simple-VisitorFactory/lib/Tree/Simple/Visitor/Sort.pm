
package Tree::Simple::Visitor::Sort;

use strict;
use warnings;

our $VERSION = '0.03';

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

sub REVERSE              { sub ($$) { $_[1]->getNodeValue()     cmp $_[0]->getNodeValue()     }};
sub NUMERIC              { sub ($$) { $_[0]->getNodeValue()     <=> $_[1]->getNodeValue()     }};
sub REVERSE_NUMERIC      { sub ($$) { $_[1]->getNodeValue()     <=> $_[0]->getNodeValue()     }};
sub ALPHABETICAL         { sub ($$) { lc($_[0]->getNodeValue()) cmp lc($_[1]->getNodeValue()) }};
sub REVERSE_ALPHABETICAL { sub ($$) { lc($_[1]->getNodeValue()) cmp lc($_[0]->getNodeValue()) }};

sub setSortFunction {
    my ($self, $sort_function) = @_;
    (defined($sort_function) && ref($sort_function) eq "CODE") 
        || die "Insufficient Arguments : You must supply a CODE reference for the sort function";
    $self->{sort_function} = $sort_function;
}

sub visit {
    my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
        || die "Insufficient Arguments : You must supply a valid Tree::Simple object";
        
    # No childs, nothing to sort
    return if $tree->isLeaf();
    
    my $sort_function;
    if ($self->{sort_function}) {
        $sort_function = $self->{sort_function};
    }
    else {
        # get the node filter
        my $filter_func = $self->getNodeFilter();
        if ($filter_func) {
            $sort_function = sub { $filter_func->($a) cmp $filter_func->($b) };
        }
        else {
            $sort_function = sub { $a->getNodeValue() cmp $b->getNodeValue() };    
        }
    }  
    
    # otherwise sort them
    $self->_sortTree($sort_function, $tree);
}

sub _sortTree {
    my ($self, $sort_function, $tree) = @_;
    
    # sort children, using the sort filter 
    my @childs = sort { $sort_function->($a, $b) } $tree->getAllChildren();
    
    # Create the new sequence
    foreach my $child (@childs) {
        # get the removed child
        $child = $tree->removeChild($child);
        # and be sure that is the one
        # we re-insert
        $tree->addChild($child);
        # only sort the child if 
        # it is not a leaf
        $self->_sortTree($sort_function, $child) unless $child->isLeaf();
    }
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::Sort - A Visitor for sorting a Tree::Simple object heirarchy

=head1 SYNOPSIS

  use Tree::Simple::Visitor::Sort;
  
  # create a visitor object
  my $visitor = Tree::Simple::Visitor::Sort->new();
  
  $tree->accept($visitor);
  # the tree is now sorted ascii-betically

  # set the sort function to 
  # use a numeric comparison
  $visitor->setSortFunction($visitor->NUMERIC);
  
  $tree->accept($visitor);
  # the tree is now sorted numerically  
  
  # set a custom sort function
  $visitor->setSortFunction(sub {
        my ($left, $right) = @_;
        lc($left->getNodeValue()->{name}) cmp lc($right->getNodeValue()->{name});
  });
  
  $tree->accept($visitor);
  # the tree's node are now sorted appropriately   

=head1 DESCRIPTION

This implements a recursive multi-level sort of a Tree::Simple heirarchy. I think this deserves some more explaination, and the best way to do that is visually. 

Given the tree:
	
    1
        1.3
        1.2
            1.2.2
            1.2.1
        1.1
    4
        4.1
    2
        2.1
    3
        3.3
        3.2
        3.1

A normal sort would produce the following tree:

    1
        1.1
        1.2
            1.2.1
            1.2.2
        1.3
    2
        2.1
    3
        3.1
        3.2
        3.3
    4
        4.1  
        
A sort using the built-in REVERSE sort function would produce the following tree:

    4
        4.1         
    3
        3.3
        3.2
        3.1
    2
        2.1
    1
        1.3
        1.2
            1.2.2
            1.2.1
        1.1

As you can see, no node is moved up or down from it's current depth, but sorted with it's siblings. Flexible customized sorting is possible within this framework, however, this cannot be used for tree-balancing or anything as complex as that. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> and C<setSortFunction> methods to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to include the trunk of the tree in the sort as well. 

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as it's C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are sorted. This can be used to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<setSortFunction ($sort_function)>

This method accepts a CODE reference as it's C<$sort_function> argument and throws an exception if it is not a code reference.  The C<$sort_function> is used by perl's builtin C<sort> routine to sort each level of the tree. The C<$sort_function> is passed two Tree::Simple objects, and must return 1 (greater than), 0 (equal to) or -1 (less than). The sort function will override and bypass any node filters which have been applied (see C<setNodeFilter> method above), they cannot be used together.

Several pre-built sort functions are provided. All of these functions assume that calling C<getNodeValue> on the Tree::Simple object will return a suitable sortable value.

=over 4

=item REVERSE

This is the reverse of the normal sort using C<cmp>. 

=item NUMERIC

This uses the numeric comparison operator C<E<lt>=E<gt>> to sort.

=item REVERSE_NUMERIC

The reverse of the above.

=item ALPHABETICAL

This lowercases the node value before using C<cmp> to sort. This results in a true alphabetical sorting.

=item REVERSE_ALPHABETICAL 

The reverse of the above.

=back

If you need to implement one of these sorting routines, but need special handling of your Tree::Simple objects (such as would be done with a node filter), I suggest you read the source code and copy and modify your own sort routine. If it is requested enough I will provide this feature in future versions, but for now I am not sure there is a large need.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

It should be noted that this is a I<destructive> action, since the sort happens I<in place> and does not produce a copy of the tree.

=back

=head1 BUGS

None that I am aware of. Of course, if you find a bug, let me know, and I will be sure to fix it. 

=head1 CODE COVERAGE

See the B<CODE COVERAGE> section in L<Tree::Simple::VisitorFactory> for more inforamtion.

=head1 SEE ALSO

These Visitor classes are all subclasses of B<Tree::Simple::Visitor>, which can be found in the B<Tree::Simple> module, you should refer to that module for more information.

=head1 ACKNOWLEDGEMENTS

=over 4

=item Thanks to Vitor Mori for the idea and much of the code for this Visitor.

=back

=head1 AUTHORS

Vitor Mori, E<lt>vvvv767@hotmail.comE<gt>

stevan little, E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004, 2005 by Vitor Mori & Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

