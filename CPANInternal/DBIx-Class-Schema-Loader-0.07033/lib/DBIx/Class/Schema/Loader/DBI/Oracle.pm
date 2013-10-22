package DBIx::Class::Schema::Loader::DBI::Oracle;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault';
use mro 'c3';
use Try::Tiny;
use namespace::clean;

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Oracle - DBIx::Class::Schema::Loader::DBI
Oracle Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    my ($current_schema) = $self->dbh->selectrow_array('SELECT USER FROM DUAL');

    $self->db_schema([ $current_schema ]) unless $self->db_schema;

    if (@{ $self->db_schema } == 1 && $self->db_schema->[0] ne '%'
        && lc($self->db_schema->[0]) ne lc($current_schema)) {
        $self->dbh->do('ALTER SESSION SET current_schema=' . $self->db_schema->[0]);
    }

    if (not defined $self->preserve_case) {
        $self->preserve_case(0);
    }
    elsif ($self->preserve_case) {
        $self->schema->storage->sql_maker->quote_char('"');
        $self->schema->storage->sql_maker->name_sep('.');
    }
}

sub _build_name_sep { '.' }

sub _system_schemas {
    my $self = shift;

    # From http://www.adp-gmbh.ch/ora/misc/known_schemas.html

    return ($self->next::method(@_), qw/ANONYMOUS APEX_PUBLIC_USER APEX_030200 APPQOSSYS CTXSYS DBSNMP DIP DMSYS EXFSYS LBACSYS MDDATA MDSYS MGMT_VIEW OLAPSYS ORACLE_OCM ORDDATA ORDPLUGINS ORDSYS OUTLN SI_INFORMTN_SCHEMA SPATIAL_CSW_ADMIN_USR SPATIAL_WFS_ADMIN_USR SYS SYSMAN SYSTEM TRACESRV MTSSYS OASPUBLIC OWBSYS OWBSYS_AUDIT WEBSYS WK_PROXY WKSYS WK_TEST WMSYS XDB OSE$HTTP$ADMIN AURORA$JIS$UTILITY$ AURORA$ORB$UNAUTHENTICATED/, qr/^FLOWS_\d\d\d\d\d\d\z/);
}

sub _system_tables {
    my $self = shift;

    return ($self->next::method(@_), 'PLAN_TABLE');
}

sub _dbh_tables {
    my ($self, $schema) = @_;

    return $self->dbh->tables(undef, $schema, '%', 'TABLE,VIEW');
}

sub _filter_tables {
    my $self = shift;

    # silence a warning from older DBD::Oracles in tests
    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
    local $SIG{__WARN__} = sub {
        $warn_handler->(@_)
        unless $_[0] =~ /^Field \d+ has an Oracle type \(\d+\) which is not explicitly supported/;
    };

    return $self->next::method(@_);
}

sub _table_fk_info {
    my $self = shift;
    my ($table) = @_;

    my $rels = $self->next::method(@_);

    my $deferrable_sth = $self->dbh->prepare_cached(<<'EOF');
select deferrable from all_constraints
where owner = ? and table_name = ? and constraint_name = ?
EOF

    foreach my $rel (@$rels) {
        # Oracle does not have update rules
        $rel->{attrs}{on_update} = 'NO ACTION';;

        # DBD::Oracle's foreign_key_info does not return DEFERRABILITY, so we get it ourselves
        my ($deferrable) = $self->dbh->selectrow_array(
            $deferrable_sth, undef, $table->schema, $table->name, $rel->{_constraint_name}
        );

        $rel->{attrs}{is_deferrable} = $deferrable && $deferrable =~ /^DEFERRABLE/i ? 1 : 0;
    }

    return $rels;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $sth = $self->dbh->prepare_cached(<<'EOF', {}, 1);
SELECT ac.constraint_name, acc.column_name
FROM all_constraints ac, all_cons_columns acc
WHERE acc.table_name=? AND acc.owner = ?
    AND ac.table_name = acc.table_name AND ac.owner = acc.owner
    AND acc.constraint_name = ac.constraint_name
    AND ac.constraint_type='U'
ORDER BY acc.position
EOF

    $sth->execute($table->name, $table->schema);

    my %constr_names;

    while(my $constr = $sth->fetchrow_arrayref) {
        my $constr_name = $self->_lc($constr->[0]);
        my $constr_col  = $self->_lc($constr->[1]);
        push @{$constr_names{$constr_name}}, $constr_col;
    }

    my @uniqs = map { [ $_ => $constr_names{$_} ] } keys %constr_names;
    return \@uniqs;
}

sub _table_comment {
    my $self = shift;
    my ($table) = @_;

    my $table_comment = $self->next::method(@_);

    return $table_comment if $table_comment;

    ($table_comment) = $self->dbh->selectrow_array(<<'EOF', {}, $table->schema, $table->name);
SELECT comments FROM all_tab_comments
WHERE owner = ?
  AND table_name = ?
  AND (table_type = 'TABLE' OR table_type = 'VIEW')
EOF

    return $table_comment
}

sub _column_comment {
    my $self = shift;
    my ($table, $column_number, $column_name) = @_;

    my $column_comment = $self->next::method(@_);

    return $column_comment if $column_comment;

    ($column_comment) = $self->dbh->selectrow_array(<<'EOF', {}, $table->schema, $table->name, $self->_uc($column_name));
SELECT comments FROM all_col_comments
WHERE owner = ?
  AND table_name = ?
  AND column_name = ?
EOF

    return $column_comment
}

sub _columns_info_for {
    my $self = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    local $self->dbh->{LongReadLen} = 1_000_000;
    local $self->dbh->{LongTruncOk} = 1;

    my $sth = $self->dbh->prepare_cached(<<'EOF', {}, 1);
SELECT trigger_body
FROM all_triggers
WHERE table_name = ? AND table_owner = ?
AND upper(trigger_type) LIKE '%BEFORE EACH ROW%' AND lower(triggering_event) LIKE '%insert%'
EOF

    $sth->execute($table->name, $table->schema);

    while (my ($trigger_body) = $sth->fetchrow_array) {
        if (my ($seq_schema, $seq_name) = $trigger_body =~ /(?:\."?(\w+)"?)?"?(\w+)"?\.nextval/i) {
            if (my ($col_name) = $trigger_body =~ /:new\.(\w+)/i) {
                $col_name = $self->_lc($col_name);

                $result->{$col_name}{is_auto_increment} = 1;

                $seq_schema = $self->_lc($seq_schema || $table->schema);
                $seq_name   = $self->_lc($seq_name);

                $result->{$col_name}{sequence} = ($self->qualify_objects ? ($seq_schema . '.') : '') . $seq_name;
            }
        }
    }

    while (my ($col, $info) = each %$result) {
        no warnings 'uninitialized';

        my $sth = $self->dbh->prepare_cached(<<'EOF', {}, 1);
SELECT data_type, data_length
FROM all_tab_columns
WHERE column_name = ? AND table_name = ? AND owner = ?
EOF
        $sth->execute($self->_uc($col), $table->name, $table->schema);
        my ($data_type, $data_length) = $sth->fetchrow_array;
        $sth->finish;
        $data_type = lc $data_type;

        if ($data_type =~ /^(?:n(?:var)?char2?|u?rowid|nclob|timestamp\(\d+\)(?: with(?: local)? time zone)?|binary_(?:float|double))\z/i) {
            $info->{data_type} = $data_type;

            if ($data_type =~ /^u?rowid\z/i) {
                $info->{size} = $data_length;
            }
        }

        if ($info->{data_type} =~ /^(?:n?[cb]lob|long(?: raw)?|bfile|date|binary_(?:float|double)|rowid)\z/i) {
            delete $info->{size};
        }

        if ($info->{data_type} =~ /^n(?:var)?char2?\z/i) {
            if (ref $info->{size}) {
                $info->{size} = $info->{size}[0] / 8;
            }
            else {
                $info->{size} = $info->{size} / 2;
            }
        }
        elsif ($info->{data_type} =~ /^(?:var)?char2?\z/i) {
            if (ref $info->{size}) {
                $info->{size} = $info->{size}[0];
            }
        }
        elsif (lc($info->{data_type}) =~ /^(?:number|decimal)\z/i) {
            $info->{original}{data_type} = 'number';
            $info->{data_type}           = 'numeric';

            if (try { $info->{size}[0] == 38 && $info->{size}[1] == 0 }) {
                $info->{original}{size} = $info->{size};

                $info->{data_type} = 'integer';
                delete $info->{size};
            }
        }
        elsif (my ($precision) = $info->{data_type} =~ /^timestamp\((\d+)\)(?: with (?:local )?time zone)?\z/i) {
            $info->{data_type} = join ' ', $info->{data_type} =~ /[a-z]+/ig;

            if ($precision == 6) {
                delete $info->{size};
            }
            else {
                $info->{size} = $precision;
            }
        }
        elsif ($info->{data_type} =~ /timestamp/i && ref $info->{size} && $info->{size}[0] == 0) {
            my $size = $info->{size}[1];
            delete $info->{size};
            $info->{size} = $size unless $size == 6;
        }
        elsif (($precision) = $info->{data_type} =~ /^interval year\((\d+)\) to month\z/i) {
            $info->{data_type} = join ' ', $info->{data_type} =~ /[a-z]+/ig;

            if ($precision == 2) {
                delete $info->{size};
            }
            else {
                $info->{size} = $precision;
            }
        }
        elsif (my ($day_precision, $second_precision) = $info->{data_type} =~ /^interval day\((\d+)\) to second\((\d+)\)\z/i) {
            $info->{data_type} = join ' ', $info->{data_type} =~ /[a-z]+/ig;

            if ($day_precision == 2 && $second_precision == 6) {
                delete $info->{size};
            }
            else {
                $info->{size} = [ $day_precision, $second_precision ];
            }
        }
        elsif ($info->{data_type} =~ /^interval year to month\z/i && ref $info->{size}) {
            my $precision = $info->{size}[0];

            if ($precision == 2) {
                delete $info->{size};
            }
            else {
                $info->{size} = $precision;
            }
        }
        elsif ($info->{data_type} =~ /^interval day to second\z/i && ref $info->{size}) {
            if ($info->{size}[0] == 2 && $info->{size}[1] == 6) {
                delete $info->{size};
            }
        }
        elsif (lc($info->{data_type}) eq 'float') {
            $info->{original}{data_type} = 'float';
            $info->{original}{size}      = $info->{size};

            if ($info->{size} <= 63) {
                $info->{data_type} = 'real';
            }
            else {
                $info->{data_type} = 'double precision';
            }
            delete $info->{size};
        }
        elsif (lc($info->{data_type}) eq 'double precision') {
            $info->{original}{data_type} = 'float';

            my $size = try { $info->{size}[0] };

            $info->{original}{size} = $size;

            if ($size <= 63) {
                $info->{data_type} = 'real';
            }
            delete $info->{size};
        }
        elsif (lc($info->{data_type}) eq 'urowid' && $info->{size} == 4000) {
            delete $info->{size};
        }
        elsif ($info->{data_type} eq '-9104') {
            $info->{data_type} = 'rowid';
            delete $info->{size};
        }
        elsif ($info->{data_type} eq '-2') {
            $info->{data_type} = 'raw';
            $info->{size} = try { $info->{size}[0] / 2 };
        }
        elsif (lc($info->{data_type}) eq 'date') {
            $info->{data_type}           = 'datetime';
            $info->{original}{data_type} = 'date';
        }
        elsif (lc($info->{data_type}) eq 'binary_float') {
            $info->{data_type}           = 'real';
            $info->{original}{data_type} = 'binary_float';
        }
        elsif (lc($info->{data_type}) eq 'binary_double') {
            $info->{data_type}           = 'double precision';
            $info->{original}{data_type} = 'binary_double';
        }

        # DEFAULT could be missed by ::DBI because of ORA-24345
        if (not defined $info->{default_value}) {
            local $self->dbh->{LongReadLen} = 1_000_000;
            local $self->dbh->{LongTruncOk} = 1;
            my $sth = $self->dbh->prepare_cached(<<'EOF', {}, 1);
SELECT data_default
FROM all_tab_columns
WHERE column_name = ? AND table_name = ? AND owner = ?
EOF
            $sth->execute($self->_uc($col), $table->name, $table->schema);
            my ($default) = $sth->fetchrow_array;
            $sth->finish;

            # this is mostly copied from ::DBI::QuotedDefault
            if (defined $default) {
                s/^\s+//, s/\s+\z// for $default;

                if ($default =~ /^'(.*?)'\z/) {
                    $info->{default_value} = $1;
                }
                elsif ($default =~ /^(-?\d.*?)\z/) {
                    $info->{default_value} = $1;
                }
                elsif ($default =~ /^NULL\z/i) {
                    my $null = 'null';
                    $info->{default_value} = \$null;
                }
                elsif ($default ne '') {
                    my $val = $default;
                    $info->{default_value} = \$val;
                }
            }
        }

        if ((try { lc(${ $info->{default_value} }) }||'') eq 'sysdate') {
            my $current_timestamp  = 'current_timestamp';
            $info->{default_value} = \$current_timestamp;

            my $sysdate = 'sysdate';
            $info->{original}{default_value} = \$sysdate;
        }
    }

    return $result;
}

sub _dbh_column_info {
    my $self  = shift;
    my ($dbh) = @_;

    # try to avoid ORA-24345
    local $dbh->{LongReadLen} = 1_000_000;
    local $dbh->{LongTruncOk} = 1;

    return $self->next::method(@_);
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
