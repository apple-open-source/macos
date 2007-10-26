
package Tree::Simple::Visitor::FindByPath;

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
    $self->{search_path} = undef;
    $self->{success} = 0;
    $self->SUPER::_init();
}

sub setSearchPath {
    my ($self, @path) = @_;
    (@path) || die "Insufficient Arguments : You must specify a path";
    $self->{search_path} = \@path;
}

sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object";

    # reset our success flag
    $self->{success} = 0;
        
    # get our filter function
	my $func;
    if ($self->{_filter_function}) {
        $func = sub { 
            my ($tree, $test) = @_;
            return (($self->{_filter_function}->($tree) . "") eq $test);
            };    
    }
    else {
        $func = sub { 
            my ($tree, $test) = @_;
            return (($tree->getNodeValue() . "") eq $test);
            };  
    }

    # get ready with our results
    my @results;
    
    # get our path
    my @path = @{$self->{search_path}};    

    # get our variables ready
    my $current_path;
    my $current_tree = $tree;
    
    # check to see if we have been 
    # asked to include the trunk
    if ($self->includeTrunk()) {
        # if we dont match the root of the path
        # then we have failed already and so return
        $self->setResults(()) && return 
            unless $func->($current_tree, $path[0]);
        # if we do match, then remove it off the path
        shift @path;
    }
    
    TOP: {
        # if we have no more @path we have found it
        unless (@path) {
            # store the current tree as
            # our last result
            $self->setResults(@results, $current_tree);
            # and set the sucess flag
            $self->{success} = 1;
            return;
        }
        # otherwise we need to keep looking ...
        # get the next element in the path
        $current_path = shift @path;
        # now check all the current tree's children
        # for a match
        foreach my $child ($current_tree->getAllChildren()) {
            if ($func->($child, $current_path)) {
                # if we find a match, then
                # we store the current tree 
                # in our results, and 
                push @results => $current_tree;
                # we change our current tree
                $current_tree = $child;
                # and go back to the TOP                
                goto TOP;
            }
        }
   
        # if we do not find a match, then we can fall off 
        # this block and the whole subroutine for that matter
        # since we know the match has failed.
        push @results => $current_tree 
            if (@path || $self->{success} == 0) && $current_tree != $tree;
    }
    # we do however, store the 
    # results as far as we got,
    # so that the user can maybe 
    # do something else to recover
    $self->setResults(@results);
}

sub getResult {
    my ($self) = @_;
    # if we did not succeed, then 
    # we return undef, ...
    return undef unless $self->{success};
    # otherwise we return the 
    # last in the results
    return $self->getResults()->[-1];
}

1;

__END__

=head1 NAME

Tree::Simple::Visitor::FindByPath - A Visitor for finding an element in a Tree::Simple hierarchy with a path

=head1 SYNOPSIS

  use Tree::Simple::Visitor::FindByPath;
  
  # create a visitor object
  my $visitor = Tree::Simple::Visitor::FindByPath->new();
  
  # set the search path for our tree  
  $visitor->setSearchPath(qw(1 1.2 1.2.2));
  
  # pass the visitor to a tree
  $tree->accept($visitor);
  
  # fetch the result, which will 
  # be the Tree::Simple object that 
  # we have found, or undefined
  my $result = $visitor->getResult() || die "No Tree found";
  
  # our result's node value should match 
  # the last element in our path
  print $result->getNodeValue(); # this should print 1.2.2

=head1 DESCRIPTION

Given a path and Tree::Simple hierarchy, this Visitor will attempt to find the node specified by the path. 

=head1 METHODS

=over 4

=item B<new>

There are no arguments to the constructor the object will be in its default state. You can use the C<setSearchPath> and C<setNodeFilter> methods to customize its behavior.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to include the trunk of the tree in the search as well. 

=item B<setSearchPath (@path)>

This is the path we will attempt to follow down the tree. We will do a stringified comparison of each element of the path and the current tree's node (or the value returned by the node filter if it is set).

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument and throws an exception if it is not a code reference. This code reference is used to filter the tree nodes as they are collected. This can be used to customize output, or to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<visit ($tree)>

This is the method that is used by Tree::Simple's C<accept> method. It can also be used on its own, it requires the C<$tree> argument to be a Tree::Simple object (or derived from a Tree::Simple object), and will throw and exception otherwise.

=item B<getResult>

This method will return the tree found at the specified path (set by the C<setSearchPath> method) or C<undef> if no tree is found.

=item B<getResults>

This method will return the tree's that make up the path specified in C<setSearchPath>. In the case of a failed search, this can be used to find the elements which did successfully match along the way.

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

