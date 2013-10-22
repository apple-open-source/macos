package DBIx::Class::Schema::Loader::DBI::MSSQL;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::Sybase::Common';
use mro 'c3';
use Try::Tiny;
use List::MoreUtils 'any';
use namespace::clean;

use DBIx::Class::Schema::Loader::Table::Sybase ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::MSSQL - DBIx::Class::Schema::Loader::DBI MSSQL Implementation.

=head1 DESCRIPTION

Base driver for Microsoft SQL Server, used by
L<DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server> for support
via L<DBD::Sybase> and
L<DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server> for support via
L<DBD::ODBC>.

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base> for
usage information.

=head1 CASE SENSITIVITY

Most MSSQL databases use C<CI> (case-insensitive) collation, for this reason
generated column names are lower-cased as this makes them easier to work with
in L<DBIx::Class>.

We attempt to detect the database collation at startup for any database
included in L<db_schema|DBIx::Class::Schema::Loader::Base/db_schema>, and set
the column lowercasing behavior accordingly, as lower-cased column names do not
work on case-sensitive databases.

To manually control case-sensitive mode, put:

    preserve_case => 1|0

in your Loader options.

See L<preserve_case|DBIx::Class::Schema::Loader::Base/preserve_case>.

B<NOTE:> this option used to be called C<case_sensitive_collation>, but has
been renamed to a more generic option.

=cut

sub _system_databases {
    return (qw/
        master model tempdb msdb
    /);
}

sub _system_tables {
    return (qw/
        spt_fallback_db spt_fallback_dev spt_fallback_usg spt_monitor spt_values MSreplication_options
    /);
}

sub _owners {
    my ($self, $db) = @_;

    my $owners = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT name
FROM [$db].dbo.sysusers
WHERE uid <> gid
EOF

    return grep !/^(?:#|guest|INFORMATION_SCHEMA|sys)/, @$owners;
}

sub _current_db {
    my $self = shift;
    return ($self->dbh->selectrow_array('SELECT db_name()'))[0];
}

sub _switch_db {
    my ($self, $db) = @_;
    $self->dbh->do("use [$db]");
}

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    my $current_db = $self->_current_db;

    if (ref $self->db_schema eq 'HASH') {
        if (keys %{ $self->db_schema } < 2) {
            my ($db) = keys %{ $self->db_schema };

            $db ||= $current_db;

            if ($db eq '%') {
                my $owners = $self->db_schema->{$db};

                my $db_names = $self->dbh->selectcol_arrayref(<<'EOF');
SELECT name
FROM master.dbo.sysdatabases
EOF

                my @dbs;

                foreach my $db_name (@$db_names) {
                    push @dbs, $db_name
                        unless any { $_ eq $db_name } $self->_system_databases;
                }

                $self->db_schema({});

                DB: foreach my $db (@dbs) {
                    if (not ((ref $owners eq 'ARRAY' && $owners->[0] eq '%') || $owners eq '%')) {
                        my @owners;

                        foreach my $owner (@$owners) {
                            push @owners, $owner
                                if $self->dbh->selectrow_array(<<"EOF");
SELECT name
FROM [$db].dbo.sysusers
WHERE name = @{[ $self->dbh->quote($owner) ]}
EOF
                        }

                        next DB unless @owners;

                        $self->db_schema->{$db} = \@owners;
                    }
                    else {
                        # for post-processing below
                        $self->db_schema->{$db} = '%';
                    }
                }

                $self->qualify_objects(1);
            }
            else {
                if ($db ne $current_db) {
                    $self->dbh->do("USE [$db]");

                    $self->qualify_objects(1);
                }
            }
        }
        else {
            $self->qualify_objects(1);
        }
    }
    elsif (ref $self->db_schema eq 'ARRAY' || (not defined $self->db_schema)) {
        my $owners = $self->db_schema;
        $owners ||= [ $self->dbh->selectrow_array('SELECT user_name()') ];

        $self->qualify_objects(1) if @$owners > 1;

        $self->db_schema({ $current_db => $owners });
    }

    foreach my $db (keys %{ $self->db_schema }) {
        if ($self->db_schema->{$db} eq '%') {
            $self->db_schema->{$db} = [ $self->_owners($db) ];

            $self->qualify_objects(1);
        }
    }

    if (not defined $self->preserve_case) {
        foreach my $db (keys %{ $self->db_schema }) {
            # We use the sys.databases query for the general case, and fallback to
            # databasepropertyex() if for some reason sys.databases is not available,
            # which does not work over DBD::ODBC with unixODBC+FreeTDS.
            #
            # XXX why does databasepropertyex() not work over DBD::ODBC ?
            #
            # more on collations here: http://msdn.microsoft.com/en-us/library/ms143515.aspx

            my $current_db = $self->_current_db;

            $self->_switch_db($db);

            my $collation_name =
                   (eval { $self->dbh->selectrow_array("SELECT collation_name FROM [$db].sys.databases WHERE name = @{[ $self->dbh->quote($db) ]}") })[0]
                || (eval { $self->dbh->selectrow_array("SELECT CAST(databasepropertyex(@{[ $self->dbh->quote($db) ]}, 'Collation') AS VARCHAR)") })[0];

            $self->_switch_db($current_db);

            if (not $collation_name) {
                warn <<"EOF";

WARNING: MSSQL Collation detection failed for database '$db'. Defaulting to
case-insensitive mode. Override the 'preserve_case' attribute in your Loader
options if needed.

See 'preserve_case' in
perldoc DBIx::Class::Schema::Loader::Base
EOF
                $self->preserve_case(0) unless $self->preserve_case;
            }
            else {
                my $case_sensitive = $collation_name =~ /_(?:CS|BIN2?)(?:_|\z)/;

                if ($case_sensitive && (not $self->preserve_case)) {
                    $self->preserve_case(1);
                }
                else {
                    $self->preserve_case(0);
                }
            }
        }
    }
}

sub _tables_list {
    my ($self, $opts) = @_;

    my @tables;

    while (my ($db, $owners) = each %{ $self->db_schema }) {
        foreach my $owner (@$owners) {
            my $table_names = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT table_name
FROM [$db].INFORMATION_SCHEMA.TABLES
WHERE table_schema = @{[ $self->dbh->quote($owner) ]}
EOF

            TABLE: foreach my $table_name (@$table_names) {
                next TABLE if any { $_ eq $table_name } $self->_system_tables;

                push @tables, DBIx::Class::Schema::Loader::Table::Sybase->new(
                    loader   => $self,
                    name     => $table_name,
                    database => $db,
                    schema   => $owner,
                );
            }
        }
    }

    return $self->_filter_tables(\@tables, $opts);
}

sub _table_pk_info {
    my ($self, $table) = @_;

    my $db = $table->database;

    my $pk = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT kcu.column_name
FROM [$db].INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc
JOIN [$db].INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu
    ON kcu.table_name = tc.table_name
        AND kcu.table_schema = tc.table_schema
        AND kcu.constraint_name = tc.constraint_name
WHERE tc.table_name = @{[ $self->dbh->quote($table->name) ]}
    AND tc.table_schema = @{[ $self->dbh->quote($table->schema) ]}
    AND tc.constraint_type = 'PRIMARY KEY'
ORDER BY kcu.ordinal_position
EOF

    $pk = [ map $self->_lc($_), @$pk ];

    return $pk;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT rc.constraint_name, rc.unique_constraint_schema, uk_tc.table_name,
       fk_kcu.column_name, uk_kcu.column_name, rc.delete_rule, rc.update_rule
FROM [$db].INFORMATION_SCHEMA.TABLE_CONSTRAINTS fk_tc
JOIN [$db].INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS rc
    ON rc.constraint_name = fk_tc.constraint_name
        AND rc.constraint_schema = fk_tc.table_schema
JOIN [$db].INFORMATION_SCHEMA.KEY_COLUMN_USAGE fk_kcu
    ON fk_kcu.constraint_name = fk_tc.constraint_name
        AND fk_kcu.table_name = fk_tc.table_name
        AND fk_kcu.table_schema = fk_tc.table_schema 
JOIN [$db].INFORMATION_SCHEMA.TABLE_CONSTRAINTS uk_tc
    ON uk_tc.constraint_name = rc.unique_constraint_name
        AND uk_tc.table_schema = rc.unique_constraint_schema
JOIN [$db].INFORMATION_SCHEMA.KEY_COLUMN_USAGE uk_kcu
    ON uk_kcu.constraint_name = rc.unique_constraint_name
        AND uk_kcu.ordinal_position = fk_kcu.ordinal_position
        AND uk_kcu.table_name = uk_tc.table_name
        AND uk_kcu.table_schema = rc.unique_constraint_schema
WHERE fk_tc.table_name = @{[ $self->dbh->quote($table->name) ]}
    AND fk_tc.table_schema = @{[ $self->dbh->quote($table->schema) ]}
ORDER BY fk_kcu.ordinal_position
EOF

    $sth->execute;

    my %rels;

    while (my ($fk, $remote_schema, $remote_table, $col, $remote_col,
               $delete_rule, $update_rule) = $sth->fetchrow_array) {
        push @{ $rels{$fk}{local_columns}  }, $self->_lc($col);
        push @{ $rels{$fk}{remote_columns} }, $self->_lc($remote_col);
        
        $rels{$fk}{remote_table} = DBIx::Class::Schema::Loader::Table::Sybase->new(
            loader   => $self,
            name     => $remote_table,
            database => $db,
            schema   => $remote_schema,
        ) unless exists $rels{$fk}{remote_table};

        $rels{$fk}{attrs} ||= {
            on_delete     => uc $delete_rule,
            on_update     => uc $update_rule,
            is_deferrable => 1 # constraints can be temporarily disabled, but DEFERRABLE is not supported
        };
    }

    return [ values %rels ];
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT tc.constraint_name, kcu.column_name
FROM [$db].INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc
JOIN [$db].INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu
    ON kcu.constraint_name = tc.constraint_name
        AND kcu.table_name = tc.table_name
        AND kcu.table_schema = tc.table_schema
wHERE tc.table_name = @{[ $self->dbh->quote($table->name) ]}
    AND tc.table_schema = @{[ $self->dbh->quote($table->schema) ]}
    AND tc.constraint_type = 'UNIQUE'
ORDER BY kcu.ordinal_position
EOF

    $sth->execute;

    my %uniq;

    while (my ($constr, $col) = $sth->fetchrow_array) {
        push @{ $uniq{$constr} }, $self->_lc($col);
    }

    return [ map [ $_ => $uniq{$_} ], keys %uniq ];
}

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;

    my $db = $table->database;

    my $result = $self->next::method(@_);

    # SQL Server: Ancient as time itself, but still out in the wild
    my $is_2k = $self->schema->storage->_server_info->{normalized_dbms_version} < 9;
    
    # get type info (and identity)
    my $rows = $self->dbh->selectall_arrayref($is_2k ? <<"EOF2K" : <<"EOF");
SELECT c.column_name, c.character_maximum_length, c.data_type, c.datetime_precision, c.column_default, (sc.status & 0x80) is_identity
FROM [$db].INFORMATION_SCHEMA.COLUMNS c
JOIN [$db].dbo.sysusers ss ON
    c.table_schema = ss.name
JOIN [$db].dbo.sysobjects so ON
    c.table_name = so.name
    AND so.uid = ss.uid
JOIN [$db].dbo.syscolumns sc ON
    c.column_name = sc.name
    AND sc.id = so.Id
WHERE c.table_schema = @{[ $self->dbh->quote($table->schema) ]}
    AND c.table_name = @{[ $self->dbh->quote($table->name) ]}
EOF2K
SELECT c.column_name, c.character_maximum_length, c.data_type, c.datetime_precision, c.column_default, sc.is_identity
FROM [$db].INFORMATION_SCHEMA.COLUMNS c
JOIN [$db].sys.schemas ss ON
    c.table_schema = ss.name
JOIN [$db].sys.objects so ON
      c.table_name   = so.name
    AND so.schema_id = ss.schema_id
JOIN [$db].sys.columns sc ON
    c.column_name = sc.name
    AND sc.object_id = so.object_id
WHERE c.table_schema = @{[ $self->dbh->quote($table->schema) ]}
    AND c.table_name = @{[ $self->dbh->quote($table->name) ]}
EOF

    foreach my $row (@$rows) {
        my ($col, $char_max_length, $data_type, $datetime_precision, $default, $is_identity) = @$row;
        $col = lc $col unless $self->preserve_case;
        my $info = $result->{$col} || next;

        $info->{data_type} = $data_type;

        if (defined $char_max_length) {
            $info->{size} = $char_max_length;
            $info->{size} = 0 if $char_max_length < 0;
        }

        if ($is_identity) {
            $info->{is_auto_increment} = 1;
            $info->{data_type} =~ s/\s*identity//i;
            delete $info->{size};
        }

        # fix types
        if ($data_type eq 'int') {
            $info->{data_type} = 'integer';
        }
        elsif ($data_type eq 'timestamp') {
            $info->{inflate_datetime} = 0;
        }
        elsif ($data_type =~ /^(?:numeric|decimal)\z/) {
            if (ref($info->{size}) && $info->{size}[0] == 18 && $info->{size}[1] == 0) {
                delete $info->{size};
            }
        }
        elsif ($data_type eq 'float') {
            $info->{data_type} = 'double precision';
            delete $info->{size};
        }
        elsif ($data_type =~ /^(?:small)?datetime\z/) {
            # fixup for DBD::Sybase
            if ($info->{default_value} && $info->{default_value} eq '3') {
                delete $info->{default_value};
            }
        }
        elsif ($data_type =~ /^(?:datetime(?:2|offset)|time)\z/) {
            $info->{size} = $datetime_precision;

            delete $info->{size} if $info->{size} == 7;
        }
        elsif ($data_type eq 'varchar'   && $info->{size} == 0) {
            $info->{data_type} = 'text';
            delete $info->{size};
        }
        elsif ($data_type eq 'nvarchar'  && $info->{size} == 0) {
            $info->{data_type} = 'ntext';
            delete $info->{size};
        }
        elsif ($data_type eq 'varbinary' && $info->{size} == 0) {
            $info->{data_type} = 'image';
            delete $info->{size};
        }

        if ($data_type !~ /^(?:n?char|n?varchar|binary|varbinary|numeric|decimal|float|datetime(?:2|offset)|time)\z/) {
            delete $info->{size};
        }

        if (defined $default) {
            # strip parens
            $default =~ s/^\( (.*) \)\z/$1/x;

            # Literal strings are in ''s, numbers are in ()s (in some versions of
            # MSSQL, in others they are unquoted) everything else is a function.
            $info->{default_value} =
                $default =~ /^['(] (.*) [)']\z/x ? $1 :
                    $default =~ /^\d/ ? $default : \$default;

            if ((eval { lc ${ $info->{default_value} } }||'') eq 'getdate()') {
                ${ $info->{default_value} } = 'current_timestamp';

                my $getdate = 'getdate()';
                $info->{original}{default_value} = \$getdate;
            }
        }
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server>,
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
