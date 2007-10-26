
package Tree::Simple::Visitor::FromNestedHash;

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
    $self->{hash_tree} = undef;
    $self->SUPER::_init();
}

sub setHashTree {
    my ($self, $hash_tree) = @_;
    (defined($hash_tree) && ref($hash_tree) eq 'HASH')
        || die "Insufficient Arguments : You must supply a valid HASH reference"; 
    # validate the tree ...
    # it must not be empty
    (scalar keys %{$hash_tree} == 1)
        || die "Insufficient Arguments : The hash tree provided must be a single rooted tree";
    $self->{hash_tree} = $hash_tree;    
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
    $self->_buildTree(
                    $tree, 
                    $self->{hash_tree}, 
                    $self->getNodeFilter(),
                    $self->includeTrunk()
                    );
}

sub _buildTree {
    my ($self, $tree, $hash, $node_filter, $include_trunk) = @_;
    foreach my $key (sort keys %{$hash}) {
        my $node = $key;
        $node = $node_filter->($node) if $node_filter;
        my $new_tree;
        if ($include_trunk) {
            $tree->setNodeValue($node);
            $new_tree = $tree;
        }
        else {
            $new_tree = Tree::Simple->new($node);
            $tree->addChild($new_tree);
        }
        $self->_buildTree($new_tree, $hash->{$key}, $node_filter) 
            if ref($hash->{$key}) eq 'HASH';
    }                
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::FromNestedHash - A Visitor for creating Tree::Simple objects from nested hash trees.

=head1 SYNOPSIS

  use Tree::Simple::Visitor::FromNestedHash;

  my $visitor = Tree::Simple::Visitor::FromNestedHash->new();

  # given this nested hash tree
  my $hash_tree = {
                Root => {
                        Child1 => {
                                GrandChild1 => {},
                                GrandChild2 => {}
                                },
                        Child2 => {}
                        }
                };

  # set the array tree we 
  # are going to convert
  $visitor->setHashTree($hash_tree);            

  $tree->accept($visitor);

  # this then creates the equivalent Tree::Simple object:
  # Tree::Simple->new("Root")
  #     ->addChildren(
  #         Tree::Simple->new("Child1")
  #             ->addChildren(
  #                 Tree::Simple->new("GrandChild1"),                
  #                 Tree::Simple->new("GrandChild2")
  #             ),
  #         Tree::Simple->new("Child2"),
  #     );

=head1 DESCRIPTION 

Given a tree constructed from nested hashs, this Visitor will create the equivalent Tree::Simple heirarchy. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter> methods to customize its behavior.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are created, the C<$filter_function> is passed the node value extracted from the hash prior to it being inserted into the tree being built. The C<$filter_function> is expected to return the value desired for inclusion into the tree.

=item B<setHashTree ($hash_tree)>

This method is used to set the C<$hash_tree> that our Tree::Simple heirarchy will be constructed from. It must be in the following form:

  {
    Root => {
          Child1 => {
                  GrandChild1 => {},
                  GrandChild2 => {}
                  },
          Child2 => {}
          }
  }

Basically each key in the hash is considered a node, values are ignored unless it is a hash reference with at least one key in it, in which case it is interpreted as containing the children of the node created from the key. 

The tree is validated prior being accepted, if it fails validation an execption will be thrown. The rules are as follows;

=over 4

=item The hash tree must not be empty.

It makes not sense to create a tree out of nothing, so it is assumed that this is a sign of something wrong.

=item The hash tree must be a single rooted tree.

The hash tree should have only one key in it's first level, if it has more than one, then it is not a single rooted tree.

=back

B<NOTE:> Hash keys are sorted ascii-betically before being added to the tree, this results in a somewhat more predictable hierarchy.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

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
