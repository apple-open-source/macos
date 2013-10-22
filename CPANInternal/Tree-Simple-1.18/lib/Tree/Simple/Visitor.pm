
package Tree::Simple::Visitor;

use strict;
use warnings;

our $VERSION = '1.11';

use Scalar::Util qw(blessed);
 
## class constants

use constant RECURSIVE     => 0x01;
use constant CHILDREN_ONLY => 0x10;

### constructor

sub new {
	my ($_class, $func, $depth) = @_;
	if (defined($depth)){
		($depth =~ /\d+/ && ($depth == RECURSIVE || $depth == CHILDREN_ONLY)) 
			|| die "Insufficient Arguments : Depth arguement must be either RECURSIVE or CHILDREN_ONLY";
	}    
	my $class = ref($_class) || $_class;
    # if we have not supplied a $func 
    # it is automatically RECURSIVE
    $depth = RECURSIVE unless defined $func;
	my $visitor = {
		depth => $depth || 0
		};
	bless($visitor, $class);
	$visitor->_init();
    if (defined $func) {
        $visitor->setNodeFilter($func);
        $visitor->includeTrunk(1);    
    }
	return $visitor;
}

### methods

sub _init {
	my ($self) = @_;
    $self->{_include_trunk} = 0;
    $self->{_filter_function} = undef;
    $self->{_results} = [];
}

sub includeTrunk {
    my ($self, $boolean) = @_;
    $self->{_include_trunk} = ($boolean ? 1 : 0) if defined $boolean;
    return $self->{_include_trunk};
}

# node filter methods

sub getNodeFilter {
    my ($self) = @_;
	return $self->{_filter_function}; 
}

sub clearNodeFilter {
    my ($self) = @_;
	$self->{_filter_function} = undef;     
}

sub setNodeFilter {
    my ($self, $filter_function) = @_;
	(defined($filter_function) && ref($filter_function) eq "CODE") 
		|| die "Insufficient Arguments : filter function argument must be a subroutine reference";
	$self->{_filter_function} = $filter_function; 
}

# results methods 

sub setResults {
    my ($self, @results) = @_;
    $self->{results} = \@results;
}

sub getResults {
    my ($self) = @_;
    return wantarray ?
             @{$self->{results}}
             :
             $self->{results};
}

# visit routine
sub visit {
	my ($self, $tree) = @_;
	(blessed($tree) && $tree->isa("Tree::Simple"))
		|| die "Insufficient Arguments : You must supply a valid Tree::Simple object";
    # get all things set up
	my @results;
	my $func;
    if ($self->{_filter_function}) {
        $func = sub { push @results => $self->{_filter_function}->(@_) };    
    }
    else {
        $func = sub { push @results => $_[0]->getNodeValue() }; 
    }
	# always apply the function 
	# to the tree's node
    $func->($tree) unless defined $self->{_include_trunk};
	# then recursively to all its children
	# if the object is configured that way
	$tree->traverse($func) if ($self->{depth} == RECURSIVE);
	# or just visit its immediate children
	# if the object is configured that way
	if ($self->{depth} == CHILDREN_ONLY) {
		$func->($_) foreach $tree->getAllChildren();
	}
    # now store the results we got
    $self->setResults(@results);
}


1;

__END__

=head1 NAME

Tree::Simple::Visitor - Visitor object for Tree::Simple objects

=head1 SYNOPSIS

  use Tree::Simple;
  use Tree::Simple::Visitor;
  
  # create a visitor instance
  my $visitor = Tree::Simple::Visitor->new();  							 
  
  # create a tree to visit
  my $tree = Tree::Simple->new(Tree::Simple->ROOT)
                         ->addChildren(
                             Tree::Simple->new("1.0"),
                             Tree::Simple->new("2.0")
                                         ->addChild(
                                             Tree::Simple->new("2.1.0")
                                             ),
                             Tree::Simple->new("3.0")
                             );

  # by default this will collect all the 
  # node values in depth-first order into 
  # our results 
  $tree->accept($visitor);	  
  
  # get our results and print them
  print join ", ", $visitor->getResults();  # prints "1.0, 2.0, 2.1.0, 3.0" 
  
  # for more complex node objects, you can specify 
  # a node filter which will be used to extract the
  # information desired from each node
  $visitor->setNodeFilter(sub { 
                my ($t) = @_;
                return $t->getNodeValue()->description();
                });  
                  
  # NOTE: this object has changed, but it still remains
  # backwards compatible to the older version, see the
  # DESCRIPTION section below for more details                  

=head1 DESCRIPTION

This object has been revised into what I think is more intelligent approach to Visitor objects. This is now a more suitable base class for building your own Visitors. It is also the base class for the visitors found in the B<Tree::Simple::VisitorFactory> distribution, which includes a number of useful pre-built Visitors.

While I have changed a number of things about this module, I have kept it backwards compatible to the old way of using it. So the original example code still works:

  my @accumulator;
  my $visitor = Tree::Simple::Visitor->new(sub {
                        my ($tree) = @_;  
                        push @accumlator, $tree->getNodeValue();
                        }, 
                        Tree::Simple::Visitor->RECURSIVE);
  							 
  $tree->accept($visitor);							 							 						
  
  print join ", ", @accumulator;  # prints "1.0, 2.0, 2.1.0, 3.0"
  
But is better expressed as this:

  my $visitor = Tree::Simple::Visitor->new();  							 
  $tree->accept($visitor);	  
  print join ", ", $visitor->getResults();  # prints "1.0, 2.0, 2.1.0, 3.0"  

This object is still pretty much a wrapper around the Tree::Simple C<traverse> method, and can be thought of as a depth-first traversal Visitor object.  

=head1 METHODS

=over 4

=item B<new ($func, $depth)>

The new style interface means that all arguments to the constructor are now optional. As a means of defining the usage of the old and new, when no arguments are sent to the constructor, it is assumed that the new style interface is being used. In the new style, the C<$depth> is always assumed to be equivalent to C<RECURSIVE> and the C<$func> argument can be set with C<setNodeFilter> instead. This is the recommended way of doing things now. If you have been using the old way, it is still there, and I will maintain backwards compatability for a few more version before removing it entirely. If you are using this module (and I don't even know if anyone actually is) you have been warned. Please contact me if this will be a problem.

The old style constructor documentation is retained her for reference:

The first argument to the constructor is a code reference to a function which expects a B<Tree::Simple> object as its only argument. The second argument is optional, it can be used to set the depth to which the function is applied. If no depth is set, the function is applied to the current B<Tree::Simple> instance. If C<$depth> is set to C<CHILDREN_ONLY>, then the function will be applied to the current B<Tree::Simple> instance and all its immediate children. If C<$depth> is set to C<RECURSIVE>, then the function will be applied to the current B<Tree::Simple> instance and all its immediate children, and all of their children recursively on down the tree. If no C<$depth> is passed to the constructor, then the function will only be applied to the current B<Tree::Simple> object and none of its children.

=item B<includeTrunk ($boolean)>

Based upon the value of C<$boolean>, this will tell the visitor to collect the trunk of the tree as well. It is defaulted to false (C<0>) in the new style interface, but is defaulted to true (C<1>) in the old style interface.

=item B<getNodeFilter>

This method returns the CODE reference set with C<setNodeFilter> argument.

=item B<clearNodeFilter>

This method clears node filter field.

=item B<setNodeFilter ($filter_function)>

This method accepts a CODE reference as its C<$filter_function> argument. This code reference is used to filter the tree nodes as they are collected. This can be used to customize output, or to gather specific information from a more complex tree node. The filter function should accept a single argument, which is the current Tree::Simple object.

=item B<getResults>

This method returns the accumulated results of the application of the node filter to the tree.

=item B<setResults>

This method should not really be used outside of this class, as it just would not make any sense to. It is included in this class and in this documenation to facilitate subclassing of this class for your own needs. If you desire to clear the results, then you can simply call C<setResults> with no argument.

=item B<visit ($tree)>

The C<visit> method accepts a B<Tree::Simple> and applies the function set in C<new> or C<setNodeFilter> appropriately. The results of this application can be retrieved with C<getResults>

=back

=head1 CONSTANTS

These constants are part of the old-style interface, and therefore will eventually be deprecated.

=over 4

=item B<RECURSIVE>

If passed this constant in the constructor, the function will be applied recursively down the hierarchy of B<Tree::Simple> objects. 

=item B<CHILDREN_ONLY>

If passed this constant in the constructor, the function will be applied to the immediate children of the B<Tree::Simple> object. 

=back

=head1 BUGS

None that I am aware of. The code is pretty thoroughly tested (see B<CODE COVERAGE> section in B<Tree::Simple>) and is based on an (non-publicly released) module which I had used in production systems for about 2 years without incident. Of course, if you find a bug, let me know, and I will be sure to fix it. 

=head1 SEE ALSO

I have written a set of pre-built Visitor objects, available on CPAN as B<Tree::Simple::VisitorFactory>.

=head1 AUTHOR

stevan little, E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004-2006 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut