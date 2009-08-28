package DBIx::Class::ResultClass::HashRefInflator;

use strict;
use warnings;

=head1 NAME

DBIx::Class::ResultClass::HashRefInflator

=head1 SYNOPSIS

 my $rs = $schema->resultset('CD');

 $rs->result_class('DBIx::Class::ResultClass::HashRefInflator');

=head1 DESCRIPTION

DBIx::Class is not built for speed: it's built for convenience and
ease of use. But sometimes you just need to get the data, and skip the
fancy objects. That is what this class provides.

There are two ways of using this class.

=over

=item *

Specify C<< $rs->result_class >> on a specific resultset to affect only that
resultset (and any chained off of it); or

=item *

Specify C<< __PACKAGE__->result_class >> on your source object to force all
uses of that result source to be inflated to hash-refs - this approach is not
recommended.

=back

=head1 METHODS

=head2 inflate_result

Inflates the result and prefetched data into a hash-ref using L<mk_hash>.

=cut

sub inflate_result {
    my ($self, $source, $me, $prefetch) = @_;

    return mk_hash($me, $prefetch);
}

=head2 mk_hash

This does all the work of inflating the (pre)fetched data.

=cut

sub mk_hash {
    my ($me, $rest) = @_;

    # $me is the hashref of cols/data from the immediate resultsource
    # $rest is a deep hashref of all the data from the prefetched
    # related sources.

    # to avoid emtpy has_many rels contain one empty hashref
    return undef if (not keys %$me);

    my $def;

    foreach (values %$me) {
        if (defined $_) {
            $def = 1;
            last;
        }
    }
    return undef unless $def;

    return { %$me,
        map {
          ( $_ =>
             ref($rest->{$_}[0]) eq 'ARRAY'
                 ? [ grep defined, map mk_hash(@$_), @{$rest->{$_}} ]
                 : mk_hash( @{$rest->{$_}} )
          )
        } keys %$rest
    };
}

1;
