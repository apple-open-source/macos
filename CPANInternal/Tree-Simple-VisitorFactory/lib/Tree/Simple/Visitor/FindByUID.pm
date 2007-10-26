
package Tree::Simple::Visitor::FindByUID;

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
    $self->{success} = 0;    
    $self->{UID_to_find} = undef;
    $self->SUPER::_init();
}

sub searchForUID {
    my ($self, $UID) = @_;
    (defined($UID)) || die "Insufficient Arguments : You must provide a UID to search for";
    $self->{UID_to_find} = $UID;
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

    # reset our success flag
    $self->{success} = 0;

    my $UID = $self->{UID_to_find};
    (defined($UID)) || die "Illegal Operation : You cannot search for a UID without setting one first";        
    # create our filter function
    # NOTE:
    # in order to get an immediate exit 
    # from the traversal once a match is
    # found, we use 'die'. It is a somewhat
    # unorthodox way of using this, but it
    # works. The found tree is propogated 
    # up the call chain and returned from 
    # this function.
	my $func;
    if ($self->{_filter_function}) {
        $func = sub { 
            my ($tree, $test) = @_;
            (($tree->getUID() eq $UID) &&  $self->{_filter_function}->($tree)) && die $tree;
            };    
    }
    else {
        $func = sub { 
            my ($tree, $test) = @_;
            ($tree->getUID() eq $UID) && die $tree;
            };  
    }

    # we eval this so we can catch the tree
    # match when it is thrown with 'die'
    eval {
        unless (defined($self->{traversal_method})) {
            # include the trunk in our 
            # search if needed
            $func->($tree) if $self->includeTrunk();        
            # and traverse
            $tree->traverse($func);
        }
        else {
            # include the trunk in our 
            # search if needed
            $self->{traversal_method}->includeTrunk(1) if $self->includeTrunk();
            # and visit            
            $self->{traversal_method}->setNodeFilter($func);
            $self->{traversal_method}->visit($tree);
        }  
    };
    # now see what we have ...
    if ($@) {
        # if we caught a Tree::Simple object
        # then we have found a match, and ...
        if (blessed($@) && $@->isa('Tree::Simple')) {
            # we assign it to our results
            $self->setResults($@);
            $self->{success} = 1;
        }
        # however, if it is not a Tree::Simple
        # object then it is likely a real exception
        else {
            # so we re-throw it
            die $@;
        }
    }
    else {
        # if no exception is thrown though, 
        # we failed in our search, and so we 
        # set our success flag to false
        $self->{success} = 0;
    }
}

sub getResult {
    my ($self) = @_;
    # if we did not succeed, then 
    # we return undef, ...
    return undef unless $self->{success};
    # otherwise we return the results
    return $self->getResults()->[0];
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::FindByUID - A Visitor for finding an element in a Tree::Simple hierarchy by UID

=head1 SYNOPSIS

  use Tree::Simple::Visitor::FindByUID;
  
  # create a visitor object
  my $visitor = Tree::Simple::Visitor::FindByUID->new();
  
  # set the search path for our tree  
  $visitor->searchForUID("MyTreeUID");
  
  # pass the visitor to a tree
  $tree->accept($visitor);
  
  # fetch the result, which will 
  # be the Tree::Simple object that 
  # we have found, or undefined
  my $result = $visitor->getResult() || die "No Tree found";

=head1 DESCRIPTION

Given a UID and Tree::Simple hierarchy, this Visitor will attempt to find the node with the same UID. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setNodeFilter>, C<setTraversalMethod>, C<includeTrunk> and C<searchForUID> methods to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to include the trunk of the tree in the search as well. 

=item B<setTraversalMethod ($visitor)>

By default we will use Tree::Simple's built in depth-first (pre-order) traverse method. If however, you desire the tree to be search in a different ordering, this can be accomplished using a different traversal method, you can supply a C<$visitor> object implementing that traversal type to this method (See  B<Tree::Simple::Visitor::BreadthFirstTraversal>, B<Tree::Simple::Visitor::PreOrderTraversal> and B<Tree::Simple::Visitor::PostOrderTraversal>).

=item B<searchForUID ($UID)>

This is the UID we will attempt to find within the tree.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to further check the tree nodes as they are searched and so can be used to customize search behavior. For instance, you could to check against the UID as well as some other criteria. The filter function should accept a single argument, which is the current Tree::Simple object and return either true (C<1>) on success, or false (C<0>) on failure.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getResult>

This method will return the tree found with the specified UID (set by the C<searchForUID> method) or C<undef> if no tree is found.

=back

=head1 BUGS

None that I am aware of. Of course, if you find a bug, let me know, and I will be sure to fix it. 

=head1 CODE COVERAGE

See the B<CODE COVERAGE> section in L<Tree::Simple::VisitorFactory> for more inforamtion.

=head1 SEE ALSO

These Visitor classes are all subclasses of B<Tree::Simple::Visitor>, which can be found in the B<Tree::Simple> module, you should refer to that module for more information.

=head1 ACKNOWLEDGEMENTS

=over 4

=item Thanks to Vitor Mori for the idea for this Visitor.

=back

=head1 AUTHOR

stevan little, E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004, 2005 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

