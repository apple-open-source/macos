package DBIx::Class::Schema::Loader::DBI::SQLAnywhere;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault';
use mro 'c3';
use List::MoreUtils 'any';
use namespace::clean;
use DBIx::Class::Schema::Loader::Table ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::SQLAnywhere - DBIx::Class::Schema::Loader::DBI
SQL Anywhere Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _system_schemas {
    return (qw/dbo SYS diagnostics rs_systabgroup SA_DEBUG/);
}

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    $self->preserve_case(1)
        unless defined $self->preserve_case;

    $self->schema->storage->sql_maker->quote_char('"');
    $self->schema->storage->sql_maker->name_sep('.');

    $self->db_schema([($self->dbh->selectrow_array('select user'))[0]])
        unless $self->db_schema;

    if (ref $self->db_schema eq 'ARRAY' && $self->db_schema->[0] eq '%') {
        my @users = grep { my $uname = $_; not any { $_ eq $uname } $self->_system_schemas }
            @{ $self->dbh->selectcol_arrayref('select user_name from sysuser') };

        $self->db_schema(\@users);
    }
}

sub _tables_list {
    my ($self, $opts) = @_;

    my @tables;

    foreach my $schema (@{ $self->db_schema }) {
        my $sth = $self->dbh->prepare(<<'EOF');
SELECT t.table_name name
FROM systab t
JOIN sysuser u
    ON t.creator = u.user_id
WHERE u.user_name = ?
EOF
        $sth->execute($schema);

        my @table_names = map @$_, @{ $sth->fetchall_arrayref };

        foreach my $table_name (@table_names) {
            push @tables, DBIx::Class::Schema::Loader::Table->new(
                loader  => $self,
                name    => $table_name,
                schema  => $schema,
            );
        }
    }

    return $self->_filter_tables(\@tables, $opts);
}

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    my $dbh = $self->schema->storage->dbh;

    while (my ($col, $info) = each %$result) {
        my $def = $info->{default_value};
        if (ref $def eq 'SCALAR' && $$def eq 'autoincrement') {
            delete $info->{default_value};
            $info->{is_auto_increment} = 1;
        }

        my ($user_type) = $dbh->selectrow_array(<<'EOF', {}, $table->schema, $table->name, lc($col));
SELECT ut.type_name
FROM systabcol tc
JOIN systab t
    ON tc.table_id = t.table_id
JOIN sysuser u
    ON t.creator = u.user_id
JOIN sysusertype ut
    ON tc.user_type = ut.type_id
WHERE u.user_name = ? AND t.table_name = ? AND lower(tc.column_name) = ?
EOF
        $info->{data_type} = $user_type if defined $user_type;

        if ($info->{data_type} eq 'double') {
            $info->{data_type} = 'double precision';
        }

        if ($info->{data_type} =~ /^(?:char|varchar|binary|varbinary)\z/ && ref($info->{size}) eq 'ARRAY') {
            $info->{size} = $info->{size}[0];
        }
        elsif ($info->{data_type} !~ /^(?:char|varchar|binary|varbinary|numeric|decimal)\z/) {
            delete $info->{size};
        }

        my $sth = $dbh->prepare(<<'EOF');
SELECT tc.width, tc.scale
FROM systabcol tc
JOIN systab t
    ON t.table_id = tc.table_id
JOIN sysuser u
    ON t.creator = u.user_id
WHERE u.user_name = ? AND t.table_name = ? AND lower(tc.column_name) = ?
EOF
        $sth->execute($table->schema, $table->name, lc($col));
        my ($width, $scale) = $sth->fetchrow_array;
        $sth->finish;

        if ($info->{data_type} =~ /^(?:numeric|decimal)\z/) {
            # We do not check for the default precision/scale, because they can be changed as PUBLIC database options.
            $info->{size} = [$width, $scale];
        }
        elsif ($info->{data_type} =~ /^(?:n(?:varchar|char) | varbit)\z/x) {
            $info->{size} = $width;
        }
        elsif ($info->{data_type} eq 'float') {
            $info->{data_type} = 'real';
        }

        if ((eval { lc ${ $info->{default_value} } }||'') eq 'current timestamp') {
            ${ $info->{default_value} } = 'current_timestamp';

            my $orig_deflt = 'current timestamp';
            $info->{original}{default_value} = \$orig_deflt;
        }
    }

    return $result;
}

sub _table_pk_info {
    my ($self, $table) = @_;
    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';
    my $sth = $self->dbh->prepare(qq{sp_pkeys ?, ?});
    $sth->execute($table->name, $table->schema);

    my @keydata;

    while (my $row = $sth->fetchrow_hashref) {
        push @keydata, $self->_lc($row->{column_name});
    }

    return \@keydata;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my ($local_cols, $remote_cols, $remote_table, @rels);
    my $sth = $self->dbh->prepare(<<'EOF');
SELECT fki.index_name fk_name, fktc.column_name local_column, pku.user_name remote_schema, pkt.table_name remote_table, pktc.column_name remote_column
FROM sysfkey fk
JOIN systab    pkt
    ON fk.primary_table_id = pkt.table_id
JOIN sysuser   pku
    ON pkt.creator = pku.user_id
JOIN systab    fkt
    ON fk.foreign_table_id = fkt.table_id
JOIN sysuser   fku
    ON fkt.creator = fku.user_id
JOIN sysidx    pki 
    ON fk.primary_table_id = pki.table_id  AND fk.primary_index_id    = pki.index_id
JOIN sysidx    fki 
    ON fk.foreign_table_id = fki.table_id  AND fk.foreign_index_id    = fki.index_id
JOIN sysidxcol fkic
    ON fkt.table_id        = fkic.table_id AND fki.index_id           = fkic.index_id
JOIN systabcol pktc
    ON pkt.table_id        = pktc.table_id AND fkic.primary_column_id = pktc.column_id
JOIN systabcol fktc
    ON fkt.table_id        = fktc.table_id AND fkic.column_id         = fktc.column_id
WHERE fku.user_name = ? AND fkt.table_name = ?
EOF
    $sth->execute($table->schema, $table->name);

    while (my ($fk, $local_col, $remote_schema, $remote_tab, $remote_col) = $sth->fetchrow_array) {
        push @{$local_cols->{$fk}},  $self->_lc($local_col);
        push @{$remote_cols->{$fk}}, $self->_lc($remote_col);
        $remote_table->{$fk} = DBIx::Class::Schema::Loader::Table->new(
            loader  => $self,
            name    => $remote_tab,
            schema  => $remote_schema,
        );
    }

    foreach my $fk (keys %$remote_table) {
        push @rels, {
            local_columns => $local_cols->{$fk},
            remote_columns => $remote_cols->{$fk},
            remote_table => $remote_table->{$fk},
        };
    }
    return \@rels;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $sth = $self->dbh->prepare(<<'EOF');
SELECT c.constraint_name, tc.column_name
FROM sysconstraint c
JOIN systab t
    ON c.table_object_id = t.object_id
JOIN sysuser u
    ON t.creator = u.user_id
JOIN sysidx i
    ON c.ref_object_id = i.object_id
JOIN sysidxcol ic
    ON i.table_id = ic.table_id AND i.index_id = ic.index_id
JOIN systabcol tc
    ON ic.table_id = tc.table_id AND ic.column_id = tc.column_id
WHERE c.constraint_type = 'U' AND u.user_name = ? AND t.table_name = ?
EOF
    $sth->execute($table->schema, $table->name);

    my $constraints;
    while (my ($constraint_name, $column) = $sth->fetchrow_array) {
        push @{$constraints->{$constraint_name}}, $self->_lc($column);
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
# vim:et sw=4 sts=4 tw=0:
