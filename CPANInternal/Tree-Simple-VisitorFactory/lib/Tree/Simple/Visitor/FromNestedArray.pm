
package Tree::Simple::Visitor::FromNestedArray;

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
    $self->{array_tree} = undef;
    $self->SUPER::_init();
}

sub setArrayTree {
    my ($self, $array_tree) = @_;
    (defined($array_tree) && ref($array_tree) eq 'ARRAY')
        || die "Insufficient Arguments : You must supply a valid ARRAY reference"; 
    # validate the tree ...
    # it must not be empty
    (scalar @{$array_tree} != 0)
        || die "Insufficient Arguments : The array tree provided is empty";
    # it's first element must not be an array
    (ref($array_tree->[0]) ne 'ARRAY')
        || die "Incorrect Object Type : The first value in the array tree is an array reference";
    # and it must be a single rooted tree
    (ref($array_tree->[1]) eq 'ARRAY')
        || die "Incorrect Object Type : The second value in the array tree must be an array reference"
            if defined($array_tree->[1]);
    $self->{array_tree} = $array_tree;    
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object"; 
	$self->_buildTree(
                $tree,
                # our array tree 
                $self->{array_tree}, 
                # get a node filter if we have one
                $self->getNodeFilter(), 
                # pass the value of includeTrunk too
                $self->includeTrunk()
                );                                    
}

sub _buildTree {
    my ($self, $tree, $array, $node_filter, $include_trunk) = @_;
    my $i = 0;
    while ($i < scalar @{$array}) {
        my $node = $array->[$i];
        # check to make sure we have a well formed tree
        (ref($node) ne 'ARRAY') 
            || die "Incorrect Object Type : The node value should never be an array reference";
        # filter the node if nessecary
        $node = $node_filter->($node) if defined($node_filter);
        # create the new tree
        my $new_tree;
        if ($include_trunk) {
            $tree->setNodeValue($node);
            $new_tree = $tree;
        }	
        else {
            $new_tree = Tree::Simple->new($node);
            $tree->addChild($new_tree);
        }
        # increment the index value
        $i++;
        # NOTE:
        # the value of include trunk is only 
        # passed in the recursion, so that 
        # the trunk/root can be populated, 
        # we have no more need for it after 
        # that time.
        $self->_buildTree($new_tree, $array->[$i++], $node_filter) 
            if ref($array->[$i]) eq 'ARRAY';
    }
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::FromNestedArray - A Visitor for creating Tree::Simple objects from nested array trees.

=head1 SYNOPSIS

  use Tree::Simple::Visitor::FromNestedArray;

  my $visitor = Tree::Simple::Visitor::FromNestedArray->new();

  # given this nested array tree
  my $array_tree = [ 
                    'Root', [ 
                        'Child1', [
                                'GrandChild1',
                                'GrandChild2'
                                ],
                        'Child2'
                        ]
                    ];
  # set the array tree we 
  # are going to convert
  $visitor->setArrayTree($array_tree);            

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

Given a tree constructed from nested arrays, this Visitor will create the equivalent Tree::Simple heirarchy. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter>, C<includTrunk> and C<setArrayTree> methods to customize its behavior.

=item B<includTrunk ($boolean)>

Setting the C<$boolean> value to true (C<1>) will cause the node value of the C<$tree> object passed into C<visit> to be set with the root value found in the C<$array_tree>. Setting it to false (C<0>), or not setting it, will result in the first value in the C<$array_tree> creating a new node level. 

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are created, the C<$filter_function> is passed the node value extracted from the array prior to it being inserted into the tree being built. The C<$filter_function> is expected to return the value desired for inclusion into the tree.

=item B<setArrayTree ($array_tree)>

This method is used to set the C<$array_tree> that our Tree::Simple heirarchy will be constructed from. It must be in the following form:

  [ 
    'Root', [ 
        'Child1', [
              'GrandChild1',
              'GrandChild2'
              ],
        'Child2'
      ]
  ]

Basically each element in the array is considered a node, unless it is an array reference, in which case it is interpreted as containing the children of the node created from the previous element in the array. 

The tree is validated prior being accepted, if it fails validation an execption will be thrown. The rules are as follows;

=over 4

=item The array tree must not be empty.

It makes not sense to create a tree out of nothing, so it is assumed that this is a sign of something wrong.

=item All nodes of the array tree must not be array references.

The root node is validated against this in this function, but all subsequent nodes are checked as the tree is built. Any nodes found to be array references are rejected and an exception is thrown. If you desire your node values to be array references, you can use the node filtering mechanism to acheive this as the node is filtered I<after> it is validated.

=item The array tree must be a single rooted tree.

If there is a second element in the array tree, it is assumed to be the children of the root, and therefore must be in the form of an array reference. 

=back

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
