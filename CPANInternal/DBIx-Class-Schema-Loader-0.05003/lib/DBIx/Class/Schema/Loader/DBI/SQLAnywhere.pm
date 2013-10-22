package DBIx::Class::Schema::Loader::DBI::SQLAnywhere;

use strict;
use warnings;
use namespace::autoclean;
use Class::C3;
use base qw/
    DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault
    DBIx::Class::Schema::Loader::DBI
/;
use Carp::Clan qw/^DBIx::Class/;
use List::MoreUtils 'uniq';

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::SQLAnywhere - DBIx::Class::Schema::Loader::DBI
SQL Anywhere Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

=cut

# check for IDENTITY columns
sub _columns_info_for {
    my $self   = shift;
    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        my $def = $info->{default_value};
        if (ref $def eq 'SCALAR' && $$def eq 'autoincrement') {
            delete $info->{default_value};
            $info->{is_auto_increment} = 1;
        }
    }

    return $result;
}

sub _table_pk_info {
    my ($self, $table) = @_;
    my $dbh = $self->schema->storage->dbh;
    local $dbh->{FetchHashKeyName} = 'NAME_lc';
    my $sth = $dbh->prepare(qq{sp_pkeys ?});
    $sth->execute($table);

    my @keydata;

    while (my $row = $sth->fetchrow_hashref) {
        push @keydata, lc $row->{column_name};
    }

    return \@keydata;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my ($local_cols, $remote_cols, $remote_table, @rels);
    my $dbh = $self->schema->storage->dbh;
    my $sth = $dbh->prepare(<<'EOF');
select fki.index_name fk_name, fktc.column_name local_column, pkt.table_name remote_table, pktc.column_name remote_column
from sysfkey fk
join sysidx    pki  on fk.primary_table_id = pki.table_id  and fk.primary_index_id = pki.index_id
join sysidx    fki  on fk.foreign_table_id = fki.table_id  and fk.foreign_index_id = fki.index_id
join systab    pkt  on fk.primary_table_id = pkt.table_id
join systab    fkt  on fk.foreign_table_id = fkt.table_id
join sysidxcol pkic on pki.table_id        = pkic.table_id and pki.index_id        = pkic.index_id
join sysidxcol fkic on fki.table_id        = fkic.table_id and fki.index_id        = fkic.index_id
join systabcol pktc on pkic.table_id       = pktc.table_id and pkic.column_id      = pktc.column_id
join systabcol fktc on fkic.table_id       = fktc.table_id and fkic.column_id      = fktc.column_id
where fkt.table_name = ?
EOF
    $sth->execute($table);

    while (my ($fk, $local_col, $remote_tab, $remote_col) = $sth->fetchrow_array) {
        push @{$local_cols->{$fk}},  lc $local_col;
        push @{$remote_cols->{$fk}}, lc $remote_col;
        $remote_table->{$fk} = lc $remote_tab;
    }

    foreach my $fk (keys %$remote_table) {
        push @rels, {
            local_columns => [ uniq @{ $local_cols->{$fk} } ],
            remote_columns => [ uniq @{ $remote_cols->{$fk} } ],
            remote_table => $remote_table->{$fk},
        };
    }
    return \@rels;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;
    my $sth = $dbh->prepare(<<'EOF');
select c.constraint_name, tc.column_name
from sysconstraint c
join systab t on c.table_object_id = t.object_id
join sysidx i on c.ref_object_id = i.object_id
join sysidxcol ic on i.table_id = ic.table_id and i.index_id = ic.index_id
join systabcol tc on ic.table_id = tc.table_id and ic.column_id = tc.column_id
where c.constraint_type = 'U' and t.table_name = ?
EOF
    $sth->execute($table);

    my $constraints;
    while (my ($constraint_name, $column) = $sth->fetchrow_array) {
        push @{$constraints->{$constraint_name}}, lc $column;
    }

    my @uniqs = map { [ $_ => $constraints->{$_} ] } keys %$constraints;
    return \@uniqs;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
