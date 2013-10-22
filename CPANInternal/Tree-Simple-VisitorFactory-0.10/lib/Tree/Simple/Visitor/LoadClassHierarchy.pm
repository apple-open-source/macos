
package Tree::Simple::Visitor::LoadClassHierarchy;

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
    $self->{class_to_load} = undef;
    $self->{include_methods} = 0;
    $self->SUPER::_init();    
}

sub setClass {
    my ($self, $class_to_load) = @_;
    (defined($class_to_load)) || die "Insufficient Arguments : Must provide a class to load";
    $self->{class_to_load} = $class_to_load; 
}

sub includeMethods {
    my ($self, $boolean) = @_;
    $self->{include_methods} = ($boolean ? 1 : 0) if defined $boolean;
    return $self->{include_methods};    
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
    # it must be a leaf
    ($tree->isLeaf()) || die "Illegal Operation : The tree must be a leaf node to load a class hierarchy";
    (defined $self->{class_to_load}) || die "Insufficient Arguments : Must provide a class to load";
    # get the filter
    my $filter = $self->getNodeFilter();
    # get the class to load
    my $class_to_load = ref($self->{class_to_load}) || $self->{class_to_load};
    
    # deal with the include trunk functionality
    if ($self->includeTrunk()) {
        $tree->setNodeValue(defined $filter ? $filter->($class_to_load) : $class_to_load);
    }
    else {
        my $new_tree = Tree::Simple->new(defined $filter ? $filter->($class_to_load) : $class_to_load);
        $tree->addChild($new_tree);
        if ($self->includeMethods()) {
            $self->_loadMethods($new_tree, $class_to_load, $filter);
        }        
        $tree = $new_tree;
    }
    
    # and load it recursively
    $self->_loadClass($tree, $class_to_load, $filter);
}

sub _loadClass {
    my ($self, $tree, $class_to_load, $filter) = @_;
    my @superclasses;
    {
        no strict 'refs';
        @superclasses = @{"${class_to_load}::ISA"};
    }
    foreach my $superclass (@superclasses) {
        my $new_tree = Tree::Simple->new(defined $filter ? $filter->($superclass) : $superclass);
        $tree->addChild($new_tree);
        if ($self->includeMethods()) {
            $self->_loadMethods($new_tree, $superclass, $filter);
        }
        $self->_loadClass($new_tree, $superclass, $filter);
    }
}

sub _loadMethods {
    my ($self, $tree, $class, $filter) = @_;
    my @methods;
    {
        no strict 'refs';
        @methods = sort grep { defined &{"${class}::$_"} } keys %{"${class}::"};    
    }
    foreach my $method (@methods) {
        $tree->addChild(Tree::Simple->new(defined $filter ? $filter->($method) : $method));
    }
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::LoadClassHierarchy - A Visitor for loading class hierarchies into a Tree::Simple hierarchy

=head1 SYNOPSIS

  use Tree::Simple::Visitor::LoadClassHierarchy;
  
  # create an visitor
  my $visitor = Tree::Simple::Visitor::LoadClassHierarchy->new();
  
  # set class as an instance, or
  $visitor->setClass($class);
  
  # as a package name
  $visitor->setClass("My::Class");
  
  # pass our visitor to the tree
  $tree->accept($visitor);
  
  # the $tree now mirrors the inheritance hierarchy of the $class

=head1 DESCRIPTION

This visitor will traverse a class's inheritance hierarchy (through the @ISA arrays) and create a Tree::Simple hierarchy which mirrors it.

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> method to customize its behavior.

=item B<includeTrunk ($boolean)>

Setting the C<$boolean> value to true (C<1>) will cause the node value of the C<$tree> object passed into C<visit> to be set with the root value found in the class heirarchy. Setting it to false (C<0>), or not setting it, will result in the first value in the class heirarchy creating a new node level.

=item B<includeMethods ($boolean)>

Setting the C<$boolean> value to true (C<1>) will cause methods to be added as a children of the class node. Setting it to false (C<0>), or not setting it, will result in this not happening.

B<NOTE:> Methods are sorted ascii-betically before they are added to the tree. This allows a more predictable heirarchy.

=item B<setClass ($class)>

The argument C<$class> should be either a class name or an instance, it is then used as the root from which to determine the class hierarchy.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are created, the C<$filter_function> is passed the node value extracted from the hash prior to it being inserted into the tree being built. The C<$filter_function> is expected to return the value desired for inclusion into the tree.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

The C<$tree> argument which is passed to C<visit> must be a leaf node. This is because this Visitor will create all the sub-nodes for this tree. If the tree is not a leaf, an exception is thrown. We do not require the tree to be a root though, and this Visitor will not affect any nodes above the C<$tree> argument.

=back

=head1 TO DO

=over

=item Improve the C<includeMethods> functionality

I am not sure the tree this creates is the optimal tree for this situation. It is sufficient for now, until I have more of an I<actual> need for this functionality.

=item Add C<includeFullSymbolTable> functionality

This would traverse the full symbol tables and produce a detailed tree of everything it finds. This takes a lot more work, and as I have no current need for it, it remains in the TO DO list.

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

