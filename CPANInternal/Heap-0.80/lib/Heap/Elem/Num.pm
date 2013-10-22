package Heap::Elem::Num;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use Heap::Elem;

require Exporter;

@ISA = qw(Exporter Heap::Elem);

# No names exported.
@EXPORT = ( );

# Available for export: NumElem (to allocate a new Heap::Elem::Num value)
@EXPORT_OK = qw( NumElem );

$VERSION = '0.80';

sub NumElem {	# exportable synonym for new
    Heap::Elem::Num->new(@_);
}

# compare two Num elems
sub cmp {
    return $_[0][0] <=> $_[1][0];
}

1;
__END__

=head1 NAME

Heap::Elem::Num - Numeric Heap Elements

=head1 SYNOPSIS

  use Heap::Elem::Num( NumElem );
  use Heap::Fibonacci;

  my $heap = Heap::Fibonacci->new;
  my $elem;

  foreach $i ( 1..100 ) {
      $elem = NumElem( $i );
      $heap->add( $elem );
  }

  while( defined( $elem = $heap->extract_top ) ) {
      print "Smallest is ", $elem->val, "\n";
  }

=head1 DESCRIPTION

Heap::Elem::Num is used to wrap numeric values into an element
that can be managed on a heap.  The top of the heap will have
the smallest element still remaining.  (See L<Heap::Elem::NumRev>
if you want the heap to always return the largest element.)

The details of the Elem interface are described in L<Heap::Elem>.

The details of using a Heap interface are described in L<Heap>.

=head1 AUTHOR

John Macdonald, john@perlwolf.com

=head1 COPYRIGHT

Copyright 1998-2007, O'Reilly & Associates.

This code is distributed under the same copyright terms as perl itself.

=head1 SEE ALSO

Heap(3), Heap::Elem(3), Heap::Elem::NumRev(3).

=cut
