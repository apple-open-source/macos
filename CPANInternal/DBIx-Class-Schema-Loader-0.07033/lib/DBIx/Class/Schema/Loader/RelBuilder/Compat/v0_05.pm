package DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_05;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_06';
use mro 'c3';
use DBIx::Class::Schema::Loader::Utils 'array_eq';
use namespace::clean;
use Lingua::EN::Inflect::Number ();

our $VERSION = '0.07033';

sub _to_PL {
    my ($self, $name) = @_;

    return Lingua::EN::Inflect::Number::to_PL($name);
}

sub _to_S {
    my ($self, $name) = @_;

    return Lingua::EN::Inflect::Number::to_S($name);
}

sub _default_relationship_attrs { +{} }

sub _relnames_and_method {
    my ( $self, $local_moniker, $rel, $cond, $uniqs, $counters ) = @_;

    my $remote_moniker = $rel->{remote_source};
    my $remote_obj     = $self->{schema}->source( $remote_moniker );
    my $remote_class   = $self->{schema}->class(  $remote_moniker );
    my $remote_relname = $self->_remote_relname( $rel->{remote_table}, $cond);

    my $local_cols  = $rel->{local_columns};
    my $local_table = $rel->{local_table};

    # If more than one rel between this pair of tables, use the local
    # col names to distinguish
    my ($local_relname, $local_relname_uninflected);
    if ( $counters->{$remote_moniker} > 1) {
        my $colnames = lc(q{_} . join(q{_}, map lc($_), @$local_cols));
        $remote_relname .= $colnames if keys %$cond > 1;

        $local_relname = lc($local_table) . $colnames;

        $local_relname_uninflected = $local_relname;
        ($local_relname) = $self->_inflect_plural( $local_relname );
    } else {
        $local_relname_uninflected = lc $local_table;
        ($local_relname) = $self->_inflect_plural(lc $local_table);
    }

    my $remote_method = 'has_many';

    # If the local columns have a UNIQUE constraint, this is a one-to-one rel
    my $local_source = $self->{schema}->source($local_moniker);
    if (array_eq([ $local_source->primary_columns ], $local_cols) ||
            grep { array_eq($_->[1], $local_cols) } @$uniqs) {
        $remote_method = 'might_have';
        ($local_relname) = $self->_inflect_singular($local_relname_uninflected);
    }

    return ( $local_relname, $remote_relname, $remote_method );
}

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_05 - RelBuilder for
compatibility with DBIx::Class::Schema::Loader version 0.05003

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base/naming> and
L<DBIx::Class::Schema::Loader::RelBuilder>.

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
