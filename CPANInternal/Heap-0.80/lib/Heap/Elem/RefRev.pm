package Heap::Elem::RefRev;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use Heap::Elem;

require Exporter;

@ISA = qw(Exporter Heap::Elem);

# No names exported.
@EXPORT = ( );

# Available for export: RefRElem (to allocate a new Heap::Elem::RefRev value)
@EXPORT_OK = qw( RefRElem );

$VERSION = '0.80';

sub RefRElem {	# exportable synonym for new
    Heap::Elem::RefRev->new(@_);
}

# compare two RefRev elems - the objects must have a compatible cmp method
sub cmp {
    return $_[1][0]->cmp( $_[0][0] );
}

1;
__END__

=head1 NAME

Heap::Elem::RefRev - Reversed Object Reverence Heap Elements

=head1 SYNOPSIS

  use Heap::Elem::RefRev( RefRElem );
  use Heap::Fibonacci;

  my $heap = Heap::Fibonacci->new;
  my $elem;

  foreach $i ( 1..100 ) {
      $obj = myObject->new( $i );
      $elem = RefRElem( $obj );
      $heap->add( $elem );
  }

  while( defined( $elem = $heap->extract_top ) ) {
      # assume that myObject object have a method I<printable>
      print "Largest is ", $elem->val->printable, "\n";
  }

=head1 DESCRIPTION

Heap::Elem::RefRev is used to wrap object reference values into an
element that can be managed on a heap.  Each referenced object must
have a method I<cmp> which can compare itself with any of the other
objects that have references on the same heap.  These comparisons
must be consistant with normal arithmetic.  The top of the heap will
have the largest (according to I<cmp>) element still remaining.
(See L<Heap::Elem::Ref> if you want the heap to always return the
smallest element.)

The details of the Elem interface are described in L<Heap::Elem>.

The details of using a Heap interface are described in L<Heap>.

=head1 AUTHOR

John Macdonald, john@perlwolf.com

=head1 COPYRIGHT

Copyright 1998-2007, O'Reilly & Associates.

This code is distributed under the same copyright terms as perl itself.

=head1 SEE ALSO

Heap(3), Heap::Elem(3), Heap::Elem::Ref(3).

=cut
