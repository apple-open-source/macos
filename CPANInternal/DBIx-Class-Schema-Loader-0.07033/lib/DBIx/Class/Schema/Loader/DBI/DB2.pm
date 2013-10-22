package DBIx::Class::Schema::Loader::DBI::DB2;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault
    DBIx::Class::Schema::Loader::DBI
/;
use mro 'c3';

use List::MoreUtils 'any';
use namespace::clean;

use DBIx::Class::Schema::Loader::Table ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::DB2 - DBIx::Class::Schema::Loader::DBI DB2 Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _system_schemas {
    my $self = shift;

    return ($self->next::method(@_), qw/
        SYSCAT SYSIBM SYSIBMADM SYSPUBLIC SYSSTAT SYSTOOLS
    /);
}

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    my $ns = $self->name_sep;

    $self->db_schema([ $self->dbh->selectrow_array(<<"EOF", {}) ]) unless $self->db_schema;
SELECT CURRENT_SCHEMA FROM sysibm${ns}sysdummy1
EOF

    if (not defined $self->preserve_case) {
        $self->preserve_case(0);
    }
    elsif ($self->preserve_case) {
        $self->schema->storage->sql_maker->quote_char('"');
        $self->schema->storage->sql_maker->name_sep($ns);
    }
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my @uniqs;

    my $sth = $self->{_cache}->{db2_uniq} ||= $self->dbh->prepare(<<'EOF');
SELECT kcu.colname, kcu.constname, kcu.colseq
FROM syscat.tabconst as tc
JOIN syscat.keycoluse as kcu
    ON tc.constname = kcu.constname
        AND tc.tabschema = kcu.tabschema
        AND tc.tabname   = kcu.tabname
WHERE tc.tabschema = ? and tc.tabname = ? and tc.type = 'U'
EOF

    $sth->execute($table->schema, $table->name);

    my %keydata;
    while(my $row = $sth->fetchrow_arrayref) {
        my ($col, $constname, $seq) = @$row;
        push(@{$keydata{$constname}}, [ $seq, $self->_lc($col) ]);
    }
    foreach my $keyname (keys %keydata) {
        my @ordered_cols = map { $_->[1] } sort { $a->[0] <=> $b->[0] }
            @{$keydata{$keyname}};
        push(@uniqs, [ $keyname => \@ordered_cols ]);
    }

    $sth->finish;

    return \@uniqs;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $sth = $self->{_cache}->{db2_fk} ||= $self->dbh->prepare(<<'EOF');
SELECT tc.constname, sr.reftabschema, sr.reftabname,
       kcu.colname, rkcu.colname, kcu.colseq,
       sr.deleterule, sr.updaterule
FROM syscat.tabconst tc
JOIN syscat.keycoluse kcu
    ON tc.constname = kcu.constname
        AND tc.tabschema = kcu.tabschema
        AND tc.tabname = kcu.tabname
JOIN syscat.references sr
    ON tc.constname = sr.constname
        AND tc.tabschema = sr.tabschema
        AND tc.tabname = sr.tabname
JOIN syscat.keycoluse rkcu
    ON sr.refkeyname = rkcu.constname
        AND kcu.colseq = rkcu.colseq
WHERE tc.tabschema = ?
    AND tc.tabname = ?
    AND tc.type = 'F';
EOF
    $sth->execute($table->schema, $table->name);

    my %rels;

    my %rules = (
        A => 'NO ACTION',
        C => 'CASCADE',
        N => 'SET NULL',
        R => 'RESTRICT',
    );

    COLS: while (my @row = $sth->fetchrow_array) {
        my ($fk, $remote_schema, $remote_table, $local_col, $remote_col,
            $colseq, $delete_rule, $update_rule) = @row;

        if (not exists $rels{$fk}) {
            if ($self->db_schema && $self->db_schema->[0] ne '%'
                && (not any { $_ eq $remote_schema } @{ $self->db_schema })) {

                next COLS;
            }

            $rels{$fk}{remote_table} = DBIx::Class::Schema::Loader::Table->new(
                loader  => $self,
                name    => $remote_table,
                schema  => $remote_schema,
            );
        }

        $rels{$fk}{local_columns}[$colseq-1]  = $self->_lc($local_col);
        $rels{$fk}{remote_columns}[$colseq-1] = $self->_lc($remote_col);

        $rels{$fk}{attrs} ||= {
            on_delete     => $rules{$delete_rule},
            on_update     => $rules{$update_rule},
            is_deferrable => 1, # DB2 has no deferrable constraints
        };
    }

    return [ values %rels ];
}


# DBD::DB2 doesn't follow the DBI API for ->tables
sub _dbh_tables {
    my ($self, $schema) = @_;

    return $self->dbh->tables($schema ? { TABLE_SCHEM => $schema } : undef);
}

sub _columns_info_for {
    my $self = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        # check for identities
        my $sth = $self->dbh->prepare_cached(
            q{
                SELECT COUNT(*)
                FROM syscat.columns
                WHERE tabschema = ? AND tabname = ? AND colname = ?
                AND identity = 'Y' AND generated != ''
            },
            {}, 1);
        $sth->execute($table->schema, $table->name, $self->_uc($col));
        if ($sth->fetchrow_array) {
            $info->{is_auto_increment} = 1;
        }

        my $data_type = $info->{data_type};

        if ($data_type !~ /^(?:(?:var)?(?:char|graphic)|decimal)\z/i) {
            delete $info->{size};
        }

        if ($data_type eq 'double') {
            $info->{data_type} = 'double precision';
        }
        elsif ($data_type eq 'decimal') {
            no warnings 'uninitialized';

            $info->{data_type} = 'numeric';

            my @size = @{ $info->{size} || [] };

            if ($size[0] == 5 && $size[1] == 0) {
                delete $info->{size};
            }
        }
        elsif ($data_type =~ /^(?:((?:var)?char) \(\) for bit data|(long varchar) for bit data)\z/i) {
            my $base_type = lc($1 || $2);

            (my $original_type = $data_type) =~ s/[()]+ //;

            $info->{original}{data_type} = $original_type;

            if ($base_type eq 'long varchar') {
                $info->{data_type} = 'blob';
            }
            else {
                if ($base_type eq 'char') {
                    $info->{data_type} = 'binary';
                }
                elsif ($base_type eq 'varchar') {
                    $info->{data_type} = 'varbinary';
                }

                my ($size) = $self->dbh->selectrow_array(<<'EOF', {}, $table->schema, $table->name, $self->_uc($col));
SELECT length
FROM syscat.columns
WHERE tabschema = ? AND tabname = ? AND colname = ?
EOF

                $info->{size} = $size if $size;
            }
        }

        if ((eval { lc ${ $info->{default_value} } }||'') =~ /^current (date|time(?:stamp)?)\z/i) {
            my $type = lc($1);

            ${ $info->{default_value} } = 'current_timestamp';

            my $orig_deflt = "current $type";
            $info->{original}{default_value} = \$orig_deflt;
        }
    }

    return $result;
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
# vim:et sts=4 sw=4 tw=0:
