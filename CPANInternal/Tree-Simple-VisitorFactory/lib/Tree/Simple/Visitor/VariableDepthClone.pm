
package Tree::Simple::Visitor::VariableDepthClone;

use strict;
use warnings;

use Scalar::Util 'blessed';

our $VERSION = '0.03';

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
    $self->{clone_depth} = undef;
    $self->SUPER::_init();    
}

sub setCloneDepth {
    my ($self, $clone_depth) = @_;
    (defined($clone_depth)) 
    	|| die "Insufficient Arguments : you must supply a clone depth";    
    $self->{clone_depth} = $clone_depth;    
}

sub getClone {
    my ($self) = @_;
    return $self->getResults()->[0];
}

sub visit {
    my ($self, $tree) = @_;
    (blessed($tree) && $tree->isa("Tree::Simple"))
    	|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
    	
    my $filter = $self->getNodeFilter();
    
    # get a new instance of the root tree type    
    my $new_root = blessed($tree)->new($tree->ROOT);
    my $new_tree = $new_root;
    
    if ($self->includeTrunk()) {
        my $cloned_trunk = blessed($tree)->new();
        $cloned_trunk->setNodeValue(
            Tree::Simple::_cloneNode($tree->getNodeValue())
        );
        $filter->($tree, $cloned_trunk) if defined $filter;
        $new_tree->addChild($cloned_trunk);
        $new_tree = $cloned_trunk;
    }
    
    $self->_cloneTree($tree, $new_tree, $self->{clone_depth}, $filter);
    
    $self->setResults($new_root);    	    
}

sub _cloneTree {
    my ($self, $tree, $clone, $depth, $filter) = @_;
    return if $depth <= 0;
    foreach my $child ($tree->getAllChildren()) {
        my $cloned_child = blessed($child)->new();
        $cloned_child->setNodeValue(
            Tree::Simple::_cloneNode($child->getNodeValue())
        );
        $filter->($child, $cloned_child) if defined $filter;        
        $clone->addChild($cloned_child);
        $self->_cloneTree($child, $cloned_child, $depth - 1, $filter) unless $child->isLeaf();
    }
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::VariableDepthClone - A Visitor for cloning parts of Tree::Simple hierarchy

=head1 SYNOPSIS

  use Tree::Simple::Visitor::VariableDepthClone;

  # create an visitor
  my $visitor = Tree::Simple::Visitor::VariableDepthClone->new();
  
  $visitor->setCloneDepth(3);

  # pass our visitor to the tree
  $tree->accept($visitor);
  
  my $partial_tree = $visitor->getClone();

=head1 DESCRIPTION

This visitor will clone 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> method to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to include the trunk of the tree in the traversal as well. This basically means it will clone the root node as well.

=item B<setCloneDepth ($number)>

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are cloned. 

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getClone>

This method returns the cloned partial tree.

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

Copyright 2005 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

