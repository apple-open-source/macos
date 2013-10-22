
package Tree::Simple::VisitorFactory;

use strict;
use warnings;

our $VERSION = '0.10';

sub new { 
    my ($class) = @_;
    return bless \$class;
}

sub get {
    my ($class, $visitor) = @_;
    (defined($visitor)) || die "Insufficient Arguments : You must specify a Visitor to load";
    $visitor = "Tree::Simple::Visitor::$visitor";
    eval "require $visitor";
    die "Illegal Operation : Could not load Visitor ($visitor) because $@" if $@;
    return $visitor->new();
}

*getVisitor = \&get;

1;

__END__

=head1 NAME

Tree::Simple::VisitorFactory - A factory object for dispensing Visitor objects

=head1 SYNOPSIS

  use Tree::Simple::VisitorFactory;
  
  my $tf = Tree::Simple::VisitorFactory->new();
  
  my $visitor = $tf->get("PathToRoot");
  
  # or call it as a class method
  my $visitor = Tree::Simple::VisitorFactory->getVisitor("PathToRoot");

=head1 DESCRIPTION

This object is really just a factory for dispensing Tree::Simple::Visitor::* objects. It is not required to use this package in order to use all the Visitors, it is just a somewhat convienient way to avoid having to type thier long class names. 

I considered making this a Singleton, but I did not because I thought that some people might not want that. I know that I am very picky about using Singletons, especially in multiprocess environments like mod_perl, so I implemented the smallest instance I knew how to, and made sure all other methods could be called as class methods too. 

=head1 METHODS

=over 4

=item B<new>

Returns an minimal instance of this object, basically just a reference back to the package (literally, see the source if you care).

=item B<get ($visitor_type)>

Attempts to load the C<$visitor_type> and returns an instance of it if successfull. If no C<$visitor_type> is specified an exception is thrown, if C<$visitor_type> fails to load, and exception is thrown.

=item B<getVisitor ($visitor_type)>

This is an alias of C<get>.

=back

=head1 AVAILABLE VISITORS

This distibution provides a number of Visitor objects which can be loaded just by giving their name. Below is a description of the available Visitors and a sort description of what they do. I have attempted to classify the Visitors into groups which are related to their use.

This factory will load any module contained inside the B<Tree::Simple::Visitor::*> namespace. Given a name, it will attempt to C<require> the module B<Tree::Simple::Visitor::E<lt>I<Name>E<gt>.pm>. This allows others to create Visitors which can be accessed with this factory, without needed to include them in this distrobution. 

=head2 Search/Path Related Visitors 

=over 4

=item B<PathToRoot>

Given a Tree::Simple object, this Visitor will find the path back to the tree's root node. 

=item B<FindByPath>

Given a path and Tree::Simple hierarchy, this Visitor will attempt to find the node specified by the path. 

=item B<FindByUID>

Given a UID and Tree::Simple hierarchy, this Visitor will attempt to find the node with the same UID. 

=item B<FindByNodeValue>

Given a node value and Tree::Simple hierarchy, this Visitor will attempt to find the node with the same node value. 

=back

=head2 Traversal Visitors

=over 4

=item B<BreadthFirstTraversal>

This implements a breadth-first traversal of a Tree::Simple hierarchy.

=item B<PostOrderTraversal>

Post-order traversal is a variation of the depth-first traversal in which the sub-tree's are processed I<before> the parent.

=item B<PreOrderTraversal>

Pre-order traversal is a depth-first traversal method in which the sub-tree's are processed I<after> the parent.

=back

=head2 FileSystem Visitors

=over 4

=item B<LoadDirectoryTree>

This visitor can be used to load a directory tree into a Tree::Simple hierarchy.

=item B<CreateDirectoryTree>

This visitor can be used to create a set of directories and files from a Tree::Simple object hierarchy.

=back

=head2 Conversion Visitors

=over 4

=item B<FromNestedArray>

Given a tree constructed from nested arrays, this Visitor will create the equivalent Tree::Simple heirarchy. 

=item B<ToNestedArray>

Given a Tree::Simple heirarchy, this Visitor will create the equivalent tree constructed from nested arrays. 

=item B<FromNestedHash>

Given a tree constructed from nested hashs, this Visitor will create the equivalent Tree::Simple heirarchy. 

=item B<ToNestedHash>

Given a Tree::Simple heirarchy, this Visitor will create the equivalent tree constructed from nested hashes. 

=back

=head2 Reflective Visitors

=over 4

=item B<LoadClassHierarchy>

Given a class name or instance, this Visitor will create a Tree::Simple hierarchy which models the classes inheritance heirarchy.

=back

=head2 Misc. Visitors

=over 4

=item B<GetAllDescendents>

Given a Tree::Simple instance this Visitor will return all the descendents recursively on down the hierarchy.

=item B<Sort>

This implements a multi-level sort of a Tree::Simple heirarchy.

=item B<VariableDepthClone>

A Visitor for cloning parts of Tree::Simple hierarchy

=back

=head1 BUGS

None that I am aware of. Of course, if you find a bug, let me know, and I will be sure to fix it. 

=head1 CODE COVERAGE

I use B<Devel::Cover> to test the code coverage of my tests, below is the B<Devel::Cover> report on this module test suite.

 -------------------------------------------- ------ ------ ------ ------ ------ ------ ------
 File                                           stmt branch   cond    sub    pod   time  total
 -------------------------------------------- ------ ------ ------ ------ ------ ------ ------
 Tree/Simple/VisitorFactory.pm                 100.0  100.0    n/a  100.0  100.0    0.4  100.0
 Tree/Simple/Visitor/BreadthFirstTraversal.pm  100.0  100.0   66.7  100.0  100.0    2.5   96.3
 Tree/Simple/Visitor/PostOrderTraversal.pm     100.0  100.0   77.8  100.0  100.0    1.7   96.3
 Tree/Simple/Visitor/PreOrderTraversal.pm      100.0    n/a   33.3  100.0  100.0    0.7   90.5
 Tree/Simple/Visitor/CreateDirectoryTree.pm    100.0   85.7   86.7  100.0  100.0    3.4   95.8
 Tree/Simple/Visitor/LoadClassHierarchy.pm     100.0   73.1   33.3  100.0  100.0    4.9   89.2
 Tree/Simple/Visitor/LoadDirectoryTree.pm      100.0   89.3   85.2  100.0  100.0   26.1   94.7
 Tree/Simple/Visitor/FindByNodeValue.pm        100.0  100.0   86.7  100.0  100.0    3.1   98.3
 Tree/Simple/Visitor/FindByPath.pm             100.0  100.0   66.7  100.0  100.0    1.2   97.9
 Tree/Simple/Visitor/FindByUID.pm              100.0  100.0   86.7  100.0  100.0    2.9   98.3
 Tree/Simple/Visitor/GetAllDescendents.pm      100.0  100.0   77.8  100.0  100.0    2.3   97.1
 Tree/Simple/Visitor/PathToRoot.pm             100.0   87.5   75.0  100.0  100.0    0.8   95.1
 Tree/Simple/Visitor/Sort.pm                   100.0  100.0   77.8  100.0  100.0    8.8   98.1
 Tree/Simple/Visitor/ToNestedArray.pm          100.0  100.0   66.7  100.0  100.0    1.5   96.5
 Tree/Simple/Visitor/ToNestedHash.pm           100.0  100.0   66.7  100.0  100.0    1.4   96.5
 Tree/Simple/Visitor/FromNestedArray.pm        100.0   94.4   81.8  100.0  100.0    8.1   96.6
 Tree/Simple/Visitor/FromNestedHash.pm         100.0   91.7   77.8  100.0  100.0    4.8   95.9
 Tree/Simple/Visitor/VariableDepthClone.pm     100.0  100.0   66.7  100.0  100.0   25.5   97.3
 -------------------------------------------- ------ ------ ------ ------ ------ ------ ------
 Total                                         100.0   93.8   76.3  100.0  100.0  100.0   96.1
 -------------------------------------------- ------ ------ ------ ------ ------ ------ ------

=head1 SEE ALSO

These Visitor classes are meant to work with L<Tree::Simple> hierarchies, you should refer to that module for more information.

=head1 AUTHOR

stevan little, E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004, 2005 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

