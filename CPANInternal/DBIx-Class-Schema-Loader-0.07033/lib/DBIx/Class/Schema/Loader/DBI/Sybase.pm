package DBIx::Class::Schema::Loader::DBI::Sybase;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::Sybase::Common';
use mro 'c3';
use List::MoreUtils 'any';
use namespace::clean;

use DBIx::Class::Schema::Loader::Table::Sybase ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Sybase - DBIx::Class::Schema::Loader::DBI
Sybase ASE Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

This class reblesses into the L<DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server> class for connections to MSSQL.

=cut

sub _rebless {
    my $self = shift;

    my $dbh = $self->schema->storage->dbh;
    my $DBMS_VERSION = @{$dbh->selectrow_arrayref(qq{sp_server_info \@attribute_id=1})}[2];
    if ($DBMS_VERSION =~ /^Microsoft /i) {
        $DBMS_VERSION =~ s/\s/_/g;
        my $subclass = "DBIx::Class::Schema::Loader::DBI::Sybase::$DBMS_VERSION";
        if ($self->load_optional_class($subclass) && !$self->isa($subclass)) {
            bless $self, $subclass;
            $self->_rebless;
      }
    }
}

sub _system_databases {
    return (qw/
        master model sybsystemdb sybsystemprocs tempdb
    /);
}

sub _system_tables {
    return (qw/
        sysquerymetrics
    /);
}

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    $self->preserve_case(1);

    my ($current_db) = $self->dbh->selectrow_array('SELECT db_name()');

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
                                if defined $self->_uid($db, $owner);
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
            my $owners = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT name
FROM [$db].dbo.sysusers
WHERE uid <> gid
EOF
            $self->db_schema->{$db} = $owners;

            $self->qualify_objects(1);
        }
    }
}

sub _tables_list {
    my ($self, $opts) = @_;

    my @tables;

    while (my ($db, $owners) = each %{ $self->db_schema }) {
        foreach my $owner (@$owners) {
            my ($uid) = $self->_uid($db, $owner);

            my $table_names = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT name
FROM [$db].dbo.sysobjects
WHERE uid = $uid
    AND type IN ('U', 'V')
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

sub _uid {
    my ($self, $db, $owner) = @_;

    my ($uid) = $self->dbh->selectrow_array(<<"EOF");
SELECT uid
FROM [$db].dbo.sysusers
WHERE name = @{[ $self->dbh->quote($owner) ]}
EOF

    return $uid;
}

sub _table_columns {
    my ($self, $table) = @_;

    my $db    = $table->database;
    my $owner = $table->schema;

    my $columns = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT c.name
FROM [$db].dbo.syscolumns c
JOIN [$db].dbo.sysobjects o
    ON c.id = o.id
WHERE o.name = @{[ $self->dbh->quote($table->name) ]}
    AND o.type IN ('U', 'V')
    AND o.uid  = @{[ $self->_uid($db, $owner) ]}
ORDER BY c.colid ASC
EOF

    return $columns;
}

sub _table_pk_info {
    my ($self, $table) = @_;

    my ($current_db) = $self->dbh->selectrow_array('SELECT db_name()');

    my $db = $table->database;

    $self->dbh->do("USE [$db]");

    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';

    my $sth = $self->dbh->prepare(<<"EOF");
sp_pkeys @{[ $self->dbh->quote($table->name) ]}, 
    @{[ $self->dbh->quote($table->schema) ]},
    @{[ $self->dbh->quote($db) ]}
EOF
    $sth->execute;

    my @keydata;

    while (my $row = $sth->fetchrow_hashref) {
        push @keydata, $row->{column_name};
    }

    $self->dbh->do("USE [$current_db]");

    return \@keydata;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $db    = $table->database;
    my $owner = $table->schema;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT sr.reftabid, sd2.name, sr.keycnt,
    fokey1,  fokey2,   fokey3,   fokey4,   fokey5,   fokey6,   fokey7,   fokey8,
    fokey9,  fokey10,  fokey11,  fokey12,  fokey13,  fokey14,  fokey15,  fokey16,
    refkey1, refkey2,  refkey3,  refkey4,  refkey5,  refkey6,  refkey7,  refkey8,
    refkey9, refkey10, refkey11, refkey12, refkey13, refkey14, refkey15, refkey16
FROM [$db].dbo.sysreferences sr
JOIN [$db].dbo.sysobjects so1
    ON sr.tableid = so1.id
JOIN [$db].dbo.sysusers su1
    ON so1.uid = su1.uid
JOIN master.dbo.sysdatabases sd2
    ON sr.pmrydbid = sd2.dbid
WHERE so1.name = @{[ $self->dbh->quote($table->name) ]}
    AND su1.name = @{[ $self->dbh->quote($table->schema) ]}
EOF
    $sth->execute;

    my @rels;

    REL: while (my @rel = $sth->fetchrow_array) {
        my ($remote_tab_id, $remote_db, $key_cnt) = splice @rel, 0, 3;

        my ($remote_tab_owner, $remote_tab_name) =
            $self->dbh->selectrow_array(<<"EOF");
SELECT su.name, so.name
FROM [$remote_db].dbo.sysusers su
JOIN [$remote_db].dbo.sysobjects so
    ON su.uid = so.uid
WHERE so.id = $remote_tab_id
EOF

        next REL
            unless any { $_ eq $remote_tab_owner }
                @{ $self->db_schema->{$remote_db} || [] };

        my @local_col_ids  = splice @rel, 0, 16;
        my @remote_col_ids = splice @rel, 0, 16;

        @local_col_ids  = splice @local_col_ids,  0, $key_cnt;
        @remote_col_ids = splice @remote_col_ids, 0, $key_cnt;

        my $remote_table = DBIx::Class::Schema::Loader::Table::Sybase->new(
            loader   => $self,
            name     => $remote_tab_name,
            database => $remote_db,
            schema   => $remote_tab_owner,
        );

        my $all_local_cols  = $self->_table_columns($table);
        my $all_remote_cols = $self->_table_columns($remote_table);

        my @local_cols  = map $all_local_cols->[$_-1],  @local_col_ids;
        my @remote_cols = map $all_remote_cols->[$_-1], @remote_col_ids;

        next REL if    (any { not defined $_ } @local_cols)
                    || (any { not defined $_ } @remote_cols);

        push @rels, {
            local_columns  => \@local_cols,
            remote_table   => $remote_table,
            remote_columns => \@remote_cols,
        };
    };

    return \@rels;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $db    = $table->database;
    my $owner = $table->schema;
    my $uid   = $self->_uid($db, $owner);

    my ($current_db) = $self->dbh->selectrow_array('SELECT db_name()');

    $self->dbh->do("USE [$db]");

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT si.name, si.indid, si.keycnt
FROM [$db].dbo.sysindexes si
JOIN [$db].dbo.sysobjects so
    ON si.id = so.id
WHERE so.name = @{[ $self->dbh->quote($table->name) ]}
    AND so.uid = $uid
    AND si.indid > 0
    AND si.status & 2048 <> 2048
    AND si.status2 & 2 = 2
EOF
    $sth->execute;

    my %uniqs;

    while (my ($ind_name, $ind_id, $key_cnt) = $sth->fetchrow_array) {
        COLS: foreach my $col_idx (1 .. ($key_cnt+1)) {
            my ($next_col) = $self->dbh->selectrow_array(<<"EOF");
SELECT index_col(
    @{[ $self->dbh->quote($table->name) ]},
    $ind_id, $col_idx, $uid
)
EOF
            last COLS unless defined $next_col;

            push @{ $uniqs{$ind_name} }, $next_col;
        }
    }

    my @uniqs = map { [ $_ => $uniqs{$_} ] } keys %uniqs;

    $self->dbh->do("USE [$current_db]");

    return \@uniqs;
}

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;
    my $result  = $self->next::method(@_);

    my $db    = $table->database;
    my $owner = $table->schema;
    my $uid   = $self->_uid($db, $owner);

    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';
    my $sth = $self->dbh->prepare(<<"EOF");
SELECT c.name, bt.name base_type, ut.name user_type, c.prec prec, c.scale scale, c.length len, c.cdefault dflt_id, c.computedcol comp_id, (c.status & 0x80) is_id
FROM [$db].dbo.syscolumns c
LEFT JOIN [$db].dbo.sysobjects o  ON c.id       = o.id
LEFT JOIN [$db].dbo.systypes   bt ON c.type     = bt.type
LEFT JOIN [$db].dbo.systypes   ut ON c.usertype = ut.usertype
WHERE o.name = @{[ $self->dbh->quote($table) ]}
    AND o.uid = $uid
    AND o.type IN ('U', 'V')
EOF
    $sth->execute;
    my $info = $sth->fetchall_hashref('name');

    while (my ($col, $res) = each %$result) {
        $res->{data_type} = $info->{$col}{user_type} || $info->{$col}{base_type};

        if ($info->{$col}{is_id}) {
            $res->{is_auto_increment} = 1;
        }
        $sth->finish;

        # column has default value
        if (my $default_id = $info->{$col}{dflt_id}) {
            my $sth = $self->dbh->prepare(<<"EOF");
SELECT cm.id, cm.text
FROM [$db].dbo.syscomments cm
WHERE cm.id = $default_id
EOF
            $sth->execute;

            if (my ($d_id, $default) = $sth->fetchrow_array) {
                my $constant_default = ($default =~ /^DEFAULT \s+ (\S.*\S)/ix)
                    ? $1
                    : $default;

                $constant_default = substr($constant_default, 1, length($constant_default) - 2)
                    if (   substr($constant_default, 0, 1) =~ m{['"\[]}
                        && substr($constant_default, -1)   =~ m{['"\]]});

                $res->{default_value} = $constant_default;
            }
        }

        # column is a computed value
        if (my $comp_id = $info->{$col}{comp_id}) {
            my $sth = $self->dbh->prepare(<<"EOF");
SELECT cm.id, cm.text
FROM [$db].dbo.syscomments cm
WHERE cm.id = $comp_id
EOF
            $sth->execute;
            if (my ($c_id, $comp) = $sth->fetchrow_array) {
                my $function = ($comp =~ /^AS \s+ (\S+)/ix) ? $1 : $comp;
                $res->{default_value} = \$function;

                if ($function =~ /^getdate\b/) {
                    $res->{inflate_datetime} = 1;
                }

                delete $res->{size};
                $res->{data_type} = undef;
            }
        }

        if (my $data_type = $res->{data_type}) {
            if ($data_type eq 'int') {
                $data_type = $res->{data_type} = 'integer';
            }
            elsif ($data_type eq 'decimal') {
                $data_type = $res->{data_type} = 'numeric';
            }
            elsif ($data_type eq 'float') {
                $data_type = $res->{data_type}
                    = ($info->{$col}{len} <= 4 ? 'real' : 'double precision');
            }

            if ($data_type eq 'timestamp') {
                $res->{inflate_datetime} = 0;
            }

            if ($data_type =~ /^(?:text|unitext|image|bigint|integer|smallint|tinyint|real|double|double precision|float|date|time|datetime|smalldatetime|money|smallmoney|timestamp|bit)\z/i) {
                delete $res->{size};
            }
            elsif ($data_type eq 'numeric') {
                my ($prec, $scale) = @{$info->{$col}}{qw/prec scale/};

                if (!defined $prec && !defined $scale) {
                    $data_type = $res->{data_type} = 'integer';
                    delete $res->{size};
                }
                elsif ($prec == 18 && $scale == 0) {
                    delete $res->{size};
                }
                else {
                    $res->{size} = [ $prec, $scale ];
                }
            }
            elsif ($data_type =~ /char/) {
                $res->{size} = $info->{$col}{len};

                if ($data_type =~ /^(?:unichar|univarchar)\z/i) {
                    $res->{size} /= 2;
                }
                elsif ($data_type =~ /^n(?:var)?char\z/i) {
                    my ($nchar_size) = $self->dbh->selectrow_array('SELECT @@ncharsize');

                    $res->{size} /= $nchar_size;
                }
            }
        }
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::Sybase::Common>,
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
