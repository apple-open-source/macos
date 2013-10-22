package Heap::Elem;

use strict;
use vars qw($VERSION);

$VERSION = '0.80';

sub new {
    my $class = shift;
    $class = ref($class) || $class;

    # value is undef, single scalar, or hash depending upon args
    my $val = (@_ > 1) ? { @_ }
	    : @_       ? $_[0]
	    :            undef;

    # two slot array, 0 for the element's own value, 1 for use by Heap
    my $self = [ $val, undef ];

    return bless $self, $class;
}


# get or set value slot
sub val {
    @_ > 1 ? ($_[0][0] = $_[1]) : $_[0][0];
}

# get or set heap slot
sub heap {
    @_ > 1 ? ($_[0][1] = $_[1]) : $_[0][1];
}

sub cmp {
    die "This cmp method must be superceded by one that knows how to compare elements."
}

1;
__END__

=head1 NAME

Heap::Elem - Base class for elements in a Heap

=head1 SYNOPSIS

  use Heap::Elem::SomeInheritor;

  use Heap::SomeHeapClass;

  $elem = Heap::Elem::SomeInheritor->new( $value );
  $heap = Heap::SomeHeapClass->new;

  $heap->add($elem);

=head1 DESCRIPTION

This is an inheritable class for Heap Elements.  It provides
the interface documentation and some inheritable methods.
Only a child classes can be used - this class is not complete.

=head1 METHODS

=over 4

=item $elem = Heap::Elem::SomeInheritor->new( [args] );

Creates a new Elem.
If there is exactly one arg, the Elem's value will be set
to that value.
If there is more than one arg provided, the Elem's value will be set
to an anonymous hash initialized to the provided args (which must
have an even number, of course).

=item $elem->heap( $val ); $elem->heap;

Provides a method for use by the Heap processing routines.
If a value argument is provided, it will be saved.  The
new saved value is always returned.  If no value argument
is provided, the old saved value is returned.

The Heap processing routines use this method to map an element
into its internal structure.  This is needed to support the
Heap methods that affect elements that are not are the top
of the heap - I<decrease_key> and I<delete>.

The Heap processing routines will ensure that this value is
undef when this elem is removed from a heap, and is not undef
after it is inserted into a heap.  This means that you can
check whether an element is currently contained within a heap
or not.  (It cannot be used to determine which heap an element
is contained in, if you have multiple heaps.  Keeping that
information accurate would make the operation of merging two
heaps into a single one take longer - it would have to traverse
all of the elements in the merged heap to update them; for
Binomial and Fibonacci heaps that would turn an O(1) operation
into an O(n) one.)

=item $elem->val( $val ); $elem->val;

Provides a method to get and/or set the value of the element.

=item $elem1->cmp($elem2)

A routine to compare two elements.  It must return a negative
value if this element should go higher on the heap than I<$elem2>,
0 if they are equal, or a positive value if this element should
go lower on the heap than I<$elem2>.  Just as with sort, the
Perl operators <=> and cmp cause the smaller value to be returned
first; similarly you can negate the meaning to reverse the order
- causing the heap to always return the largest element instead
of the smallest.

=back

=head1 INHERITING

This class can be inherited to provide an object with the
ability to be heaped.  If the object is implemented as
a hash, and if it can deal with a key of I<heap>, leaving
it unchanged for use by the heap routines, then the following
implemetation will work.

  package myObject;

  require Exporter;

  @ISA = qw(Heap::Elem);

  sub new {
      my $self = shift;
      my $class = ref($self) || $self;

      my $self = SUPER::new($class);

      # set $self->{key} = $value;
  }

  sub cmp {
      my $self = shift;
      my $other = shift;

      $self->{key} cmp $other->{key};
  }

  # other methods for the rest of myObject's functionality

=head1 AUTHOR

John Macdonald, john@perlwolf.com

=head1 COPYRIGHT

Copyright 1998-2007, O'Reilly & Associates.

This code is distributed under the same copyright terms as perl itself.

=head1 SEE ALSO

Heap(3), Heap::Elem::Num(3), Heap::Elem::NumRev(3),
Heap::Elem::Str(3), Heap::Elem::StrRev(3).

=cut
