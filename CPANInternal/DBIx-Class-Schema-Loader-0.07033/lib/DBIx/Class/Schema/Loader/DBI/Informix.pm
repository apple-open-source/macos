package DBIx::Class::Schema::Loader::DBI::Informix;

use strict;
use warnings;
use base qw/DBIx::Class::Schema::Loader::DBI/;
use mro 'c3';
use Scalar::Util 'looks_like_number';
use List::MoreUtils 'any';
use Try::Tiny;
use namespace::clean;
use DBIx::Class::Schema::Loader::Table::Informix ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Informix - DBIx::Class::Schema::Loader::DBI
Informix Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _build_name_sep { '.' }

sub _system_databases {
    return (qw/
        sysmaster sysutils sysuser sysadmin
    /);
}

sub _current_db {
    my $self = shift;

    my ($current_db) = $self->dbh->selectrow_array(<<'EOF');
SELECT rtrim(ODB_DBName)
FROM sysmaster:informix.SysOpenDB
WHERE ODB_SessionID = (
        SELECT DBINFO('sessionid')
        FROM informix.SysTables
        WHERE TabID = 1
    ) and ODB_IsCurrent = 'Y'
EOF

    return $current_db;
}

sub _owners {
    my ($self, $db) = @_;

    my ($owners) = $self->dbh->selectcol_arrayref(<<"EOF");
SELECT distinct(rtrim(owner))
FROM ${db}:informix.systables
EOF

    my @owners = grep $_ && $_ ne 'informix' && !/^\d/, @$owners;

    return @owners;
}

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    if (not defined $self->preserve_case) {
        $self->preserve_case(0);
    }
    elsif ($self->preserve_case) {
        $self->schema->storage->sql_maker->quote_char('"');
        $self->schema->storage->sql_maker->name_sep('.');
    }

    my $current_db = $self->_current_db;

    if (ref $self->db_schema eq 'HASH') {
        if (keys %{ $self->db_schema } < 2) {
            my ($db) = keys %{ $self->db_schema };

            $db ||= $current_db;

            if ($db eq '%') {
                my $owners = $self->db_schema->{$db};

                my $db_names = $self->dbh->selectcol_arrayref(<<'EOF');
SELECT rtrim(name)
FROM sysmaster:sysdatabases
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

                        my @db_owners = try {
                            $self->_owners($db);
                        }
                        catch {
                            if (/without logging/) {
                                warn
"Database '$db' is unreferencable due to lack of logging.\n";
                            }
                            return ();
                        };

                        foreach my $owner (@$owners) {
                            push @owners, $owner
                                if any { $_ eq $owner } @db_owners;
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
        $owners ||= [ $self->dbh->selectrow_array(<<'EOF') ];
SELECT rtrim(username)
FROM sysmaster:syssessions
WHERE sid = DBINFO('sessionid')
EOF

        $self->qualify_objects(1) if @$owners > 1;

        $self->db_schema({ $current_db => $owners });
    }

    DB: foreach my $db (keys %{ $self->db_schema }) {
        if ($self->db_schema->{$db} eq '%') {
            my @db_owners = try {
                $self->_owners($db);
            }
            catch {
                if (/without logging/) {
                    warn
"Database '$db' is unreferencable due to lack of logging.\n";
                }
                return ();
            };

            if (not @db_owners) {
                delete $self->db_schema->{$db};
                next DB;
            }

            $self->db_schema->{$db} = \@db_owners;

            $self->qualify_objects(1);
        }
    }
}

sub _tables_list {
    my ($self, $opts) = @_;

    my @tables;

    while (my ($db, $owners) = each %{ $self->db_schema }) {
        foreach my $owner (@$owners) {
            my $table_names = $self->dbh->selectcol_arrayref(<<"EOF", {}, $owner);
select tabname
FROM ${db}:informix.systables
WHERE rtrim(owner) = ?
EOF

            TABLE: foreach my $table_name (@$table_names) {
                next if $table_name =~ /^\s/;

                push @tables, DBIx::Class::Schema::Loader::Table::Informix->new(
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

sub _constraints_for {
    my ($self, $table, $type) = @_;

    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT c.constrname, i.*
FROM ${db}:informix.sysconstraints c
JOIN ${db}:informix.systables t
    ON t.tabid = c.tabid
JOIN ${db}:informix.sysindexes i
    ON c.idxname = i.idxname
WHERE t.tabname = ? and c.constrtype = ?
EOF
    $sth->execute($table, $type);
    my $indexes = $sth->fetchall_hashref('constrname');
    $sth->finish;

    my $cols = $self->_colnames_by_colno($table);

    my $constraints;
    while (my ($constr_name, $idx_def) = each %$indexes) {
        $constraints->{$constr_name} = $self->_idx_colnames($idx_def, $cols);
    }

    return $constraints;
}

sub _idx_colnames {
    my ($self, $idx_info, $table_cols_by_colno) = @_;

    return [ map $table_cols_by_colno->{$_}, grep $_, map $idx_info->{$_}, map "part$_", (1..16) ];
}

sub _colnames_by_colno {
    my ($self, $table) = @_;

    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT c.colname, c.colno
FROM ${db}:informix.syscolumns c
JOIN ${db}:informix.systables t
    ON c.tabid = t.tabid
WHERE t.tabname = ?
EOF
    $sth->execute($table);
    my $cols = $sth->fetchall_hashref('colno');
    $cols = { map +($_, $self->_lc($cols->{$_}{colname})), keys %$cols };

    return $cols;
}

sub _table_pk_info {
    my ($self, $table) = @_;

    my $pk = (values %{ $self->_constraints_for($table, 'P') || {} })[0];

    return $pk;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $constraints = $self->_constraints_for($table, 'U');

    my @uniqs = map { [ $_ => $constraints->{$_} ] } keys %$constraints;
    return \@uniqs;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $local_columns = $self->_constraints_for($table, 'R');

    local $self->dbh->{FetchHashKeyName} = 'NAME_lc';

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT c.constrname local_constraint, rt.tabname remote_table, rtrim(rt.owner) remote_owner, rc.constrname remote_constraint, ri.*
FROM ${db}:informix.sysconstraints c
JOIN ${db}:informix.systables t
    ON c.tabid = t.tabid
JOIN ${db}:informix.sysreferences r
    ON c.constrid = r.constrid
JOIN ${db}:informix.sysconstraints rc
    ON rc.constrid = r.primary
JOIN ${db}:informix.systables rt
    ON r.ptabid = rt.tabid
JOIN ${db}:informix.sysindexes ri
    ON rc.idxname = ri.idxname
WHERE t.tabname = ? and c.constrtype = 'R'
EOF
    $sth->execute($table);
    my $remotes = $sth->fetchall_hashref('local_constraint');
    $sth->finish;

    my @rels;

    while (my ($local_constraint, $remote_info) = each %$remotes) {
        my $remote_table = DBIx::Class::Schema::Loader::Table::Informix->new(
            loader   => $self,
            name     => $remote_info->{remote_table},
            database => $db,
            schema   => $remote_info->{remote_owner},
        );

        push @rels, {
            local_columns  => $local_columns->{$local_constraint},
            remote_columns => $self->_idx_colnames($remote_info, $self->_colnames_by_colno($remote_table)),
            remote_table   => $remote_table,
        };
    }

    return \@rels;
}

# This is directly from http://www.ibm.com/developerworks/data/zones/informix/library/techarticle/0305parker/0305parker.html
# it doesn't work at all
sub _informix_datetime_precision {
    my @date_type = qw/DUMMY year  month day   hour   minute  second  fraction(1) fraction(2) fraction(3) fraction(4) fraction(5)/;
    my @start_end = (  [],   [1,5],[5,7],[7,9],[9,11],[11,13],[13,15],[15,16],    [16,17],    [17,18],    [18,19],    [19,20]    );

    my ($self, $collength) = @_;

    my $i = ($collength % 16) + 1;
    my $j = int(($collength % 256) / 16) + 1;
    my $k = int($collength / 256);

    my $len = $start_end[$i][1] - $start_end[$j][0];
    $len = $k - $len;

    if ($len == 0 || $j > 11) {
        return $date_type[$j] . ' to ' . $date_type[$i];
    }

    $k  = $start_end[$j][1] - $start_end[$j][0];
    $k += $len;

    return $date_type[$j] . "($k) to " . $date_type[$i];
}

sub _columns_info_for {
    my $self = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    my $db = $table->database;

    my $sth = $self->dbh->prepare(<<"EOF");
SELECT c.colname, c.coltype, c.collength, c.colmin, d.type deflt_type, d.default deflt
FROM ${db}:informix.syscolumns c
JOIN ${db}:informix.systables t
    ON c.tabid = t.tabid
LEFT JOIN ${db}:informix.sysdefaults d
    ON t.tabid = d.tabid AND c.colno = d.colno
WHERE t.tabname = ?
EOF
    $sth->execute($table);
    my $cols = $sth->fetchall_hashref('colname');
    $sth->finish;

    while (my ($col, $info) = each %$cols) {
        $col = $self->_lc($col);

        my $type = $info->{coltype} % 256;

        if ($type == 6) { # SERIAL
            $result->{$col}{is_auto_increment} = 1;
        }
        elsif ($type == 7) {
            $result->{$col}{data_type} = 'date';
        }
        elsif ($type == 10) {
            $result->{$col}{data_type} = 'datetime year to fraction(5)';
            # this doesn't work yet
#                $result->{$col}{data_type} = 'datetime ' . $self->_informix_datetime_precision($info->{collength});
        }
        elsif ($type == 17 || $type == 52) {
            $result->{$col}{data_type} = 'bigint';
        }
        elsif ($type == 40) {
            $result->{$col}{data_type} = 'lvarchar';
            $result->{$col}{size}      = $info->{collength};
        }
        elsif ($type == 12) {
            $result->{$col}{data_type} = 'text';
        }
        elsif ($type == 11) {
            $result->{$col}{data_type}           = 'bytea';
            $result->{$col}{original}{data_type} = 'byte';
        }
        elsif ($type == 41) {
            # XXX no way to distinguish opaque types boolean, blob and clob
            $result->{$col}{data_type} = 'blob' unless $result->{$col}{data_type} eq 'smallint';
        }
        elsif ($type == 21) {
            $result->{$col}{data_type} = 'list';
        }
        elsif ($type == 20) {
            $result->{$col}{data_type} = 'multiset';
        }
        elsif ($type == 19) {
            $result->{$col}{data_type} = 'set';
        }
        elsif ($type == 15) {
            $result->{$col}{data_type} = 'nchar';
        }
        elsif ($type == 16) {
            $result->{$col}{data_type} = 'nvarchar';
        }
        # XXX untested!
        elsif ($info->{coltype} == 2061) {
            $result->{$col}{data_type} = 'idssecuritylabel';
        }

        my $data_type = $result->{$col}{data_type};

        if ($data_type !~ /^(?:[nl]?(?:var)?char|decimal)\z/i) {
            delete $result->{$col}{size};
        }

        if (lc($data_type) eq 'decimal') {
            no warnings 'uninitialized';

            $result->{$col}{data_type} = 'numeric';

            my @size = @{ $result->{$col}{size} || [] };

            if ($size[0] == 16 && $size[1] == -4) {
                delete $result->{$col}{size};
            }
            elsif ($size[0] == 16 && $size[1] == 2) {
                $result->{$col}{data_type} = 'money';
                delete $result->{$col}{size};
            }
        }
        elsif (lc($data_type) eq 'smallfloat') {
            $result->{$col}{data_type} = 'real';
        }
        elsif (lc($data_type) eq 'float') {
            $result->{$col}{data_type} = 'double precision';
        }
        elsif ($data_type =~ /^n?(?:var)?char\z/i) {
            $result->{$col}{size} = $result->{$col}{size}[0];
        }

        # XXX colmin doesn't work for min size of varchar columns, it's NULL
#        if (lc($data_type) eq 'varchar') {
#            $result->{$col}{size}[1] = $info->{colmin};
#        }
       
        my ($default_type, $default) = @{$info}{qw/deflt_type deflt/};

        next unless $default_type;

        if ($default_type eq 'C') {
            my $current = 'current year to fraction(5)';
            $result->{$col}{default_value} = \$current;
        }
        elsif ($default_type eq 'T') {
            my $today = 'today';
            $result->{$col}{default_value} = \$today;
        }
        else {
            $default = (split ' ', $default, 2)[-1];

            $default =~ s/\s+\z// if looks_like_number $default;

            # remove trailing 0s in floating point defaults
            # disabled, this is unsafe since it might be a varchar default
            #$default =~ s/0+\z// if $default =~ /^\d+\.\d+\z/;

            $result->{$col}{default_value} = $default;
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
# vim:et sw=4 sts=4 tw=0:
