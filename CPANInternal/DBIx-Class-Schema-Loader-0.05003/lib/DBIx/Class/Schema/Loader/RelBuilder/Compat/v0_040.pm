package DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_040;

use strict;
use warnings;
use Class::C3;

use base 'DBIx::Class::Schema::Loader::RelBuilder';

sub _uniq_fk_rel {
    my ($self, $local_moniker, $local_relname, $local_cols, $uniqs) = @_;

    return ('has_many', $local_relname);
}

sub _remote_attrs { }

sub _remote_relname {
    my ($self, $remote_table, $cond) = @_;

    my $remote_relname;
    # for single-column case, set the remote relname to the column
    # name, to make filter accessors work
    if(scalar keys %{$cond} == 1) {
        $remote_relname = $self->_inflect_singular(values %{$cond});
    }
    else {
        $remote_relname = $self->_inflect_singular(lc $remote_table);
    }

    return $remote_relname;
}

sub _multi_rel_local_relname {
    my ($self, $remote_class, $local_table, $local_cols) = @_;

    my $colnames = q{_} . join(q{_}, @$local_cols);
    my $local_relname = $self->_inflect_plural( lc($local_table) . $colnames );

    return $local_relname;
}

1;

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_040 - RelBuilder for
compatibility with DBIx::Class::Schema::Loader version 0.04006

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base/naming>.

=cut
