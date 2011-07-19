package # hide form PAUSE
    DBIx::Class::CDBICompat::AbstractSearch;

use strict;
use warnings;

=head1 NAME

DBIx::Class::CDBICompat::AbstractSearch - Emulates Class::DBI::AbstractSearch

=head1 SYNOPSIS

See DBIx::Class::CDBICompat for usage directions.

=head1 DESCRIPTION

Emulates L<Class::DBI::AbstractSearch>.

=cut

# The keys are mostly the same.
my %cdbi2dbix = (
    limit               => 'rows',
);

sub search_where {
    my $class = shift;
    my $where = (ref $_[0]) ? $_[0] : { @_ };
    my $attr  = (ref $_[0]) ? $_[1] : {};

    # Translate the keys
    $attr->{$cdbi2dbix{$_}} = delete $attr->{$_} for keys %cdbi2dbix;

    return $class->resultset_instance->search($where, $attr);
}

1;
