package # hide from PAUSE
    DBIx::Class::CDBICompat::Copy;

use strict;
use warnings;

use Carp;

=head1 NAME

DBIx::Class::CDBICompat::Copy - Emulates Class::DBI->copy($new_id)

=head1 SYNOPSIS

See DBIx::Class::CDBICompat for usage directions.

=head1 DESCRIPTION

Emulates C<<Class::DBI->copy($new_id)>>.

=cut


# CDBI's copy will take an id in addition to a hash ref.
sub copy {
    my($self, $arg) = @_;
    return $self->next::method($arg) if ref $arg;

    my @primary_columns = $self->primary_columns;
    croak("Need hash-ref to edit copied column values")
        if @primary_columns > 1;

    return $self->next::method({ $primary_columns[0] => $arg });
}

1;
