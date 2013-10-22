package DBIx::Class::Schema::Loader::DBI::Pg;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault';
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Pg - DBIx::Class::Schema::Loader::DBI
PostgreSQL Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    $self->{db_schema} ||= ['public'];

    if (not defined $self->preserve_case) {
        $self->preserve_case(0);
    }
    elsif ($self->preserve_case) {
        $self->schema->storage->sql_maker->quote_char('"');
        $self->schema->storage->sql_maker->name_sep('.');
    }
}

sub _system_schemas {
    my $self = shift;

    return ($self->next::method(@_), 'pg_catalog');
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $sth = $self->dbh->prepare_cached(<<"EOF");
SELECT rc.constraint_name, rc.unique_constraint_schema, uk_tc.table_name,
       fk_kcu.column_name, uk_kcu.column_name, rc.delete_rule, rc.update_rule,
       fk_tc.is_deferrable
FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS fk_tc
JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS rc
    ON rc.constraint_name = fk_tc.constraint_name
        AND rc.constraint_schema = fk_tc.table_schema
JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE fk_kcu
    ON fk_kcu.constraint_name = fk_tc.constraint_name
        AND fk_kcu.table_name = fk_tc.table_name
        AND fk_kcu.table_schema = fk_tc.table_schema
JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS uk_tc
    ON uk_tc.constraint_name = rc.unique_constraint_name
        AND uk_tc.table_schema = rc.unique_constraint_schema
JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE uk_kcu
    ON uk_kcu.constraint_name = rc.unique_constraint_name
        AND uk_kcu.ordinal_position = fk_kcu.ordinal_position
        AND uk_kcu.table_name = uk_tc.table_name
        AND uk_kcu.table_schema = rc.unique_constraint_schema
WHERE fk_tc.table_name = ?
    AND fk_tc.table_schema = ?
ORDER BY fk_kcu.ordinal_position
EOF

    $sth->execute($table->name, $table->schema);

    my %rels;

    while (my ($fk, $remote_schema, $remote_table, $col, $remote_col,
               $delete_rule, $update_rule, $is_deferrable) = $sth->fetchrow_array) {
        push @{ $rels{$fk}{local_columns}  }, $self->_lc($col);
        push @{ $rels{$fk}{remote_columns} }, $self->_lc($remote_col);

        $rels{$fk}{remote_table} = DBIx::Class::Schema::Loader::Table->new(
            loader   => $self,
            name     => $remote_table,
            schema   => $remote_schema,
        ) unless exists $rels{$fk}{remote_table};

        $rels{$fk}{attrs} ||= {
            on_delete     => uc $delete_rule,
            on_update     => uc $update_rule,
            is_deferrable => uc $is_deferrable eq 'YES' ? 1 : 0,
        };
    }

    return [ values %rels ];
}


sub _table_uniq_info {
    my ($self, $table) = @_;

    # Use the default support if available
    return $self->next::method($table)
        if $DBD::Pg::VERSION >= 1.50;

    my @uniqs;

    # Most of the SQL here is mostly based on
    #   Rose::DB::Object::Metadata::Auto::Pg, after some prodding from
    #   John Siracusa to use his superior SQL code :)

    my $attr_sth = $self->{_cache}->{pg_attr_sth} ||= $self->dbh->prepare(
        q{SELECT attname FROM pg_catalog.pg_attribute
        WHERE attrelid = ? AND attnum = ?}
    );

    my $uniq_sth = $self->{_cache}->{pg_uniq_sth} ||= $self->dbh->prepare(
        q{SELECT x.indrelid, i.relname, x.indkey
        FROM
          pg_catalog.pg_index x
          JOIN pg_catalog.pg_class c ON c.oid = x.indrelid
          JOIN pg_catalog.pg_class i ON i.oid = x.indexrelid
          JOIN pg_catalog.pg_constraint con ON con.conname = i.relname
          LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
        WHERE
          x.indisunique = 't' AND
          c.relkind     = 'r' AND
          i.relkind     = 'i' AND
          con.contype   = 'u' AND
          n.nspname     = ? AND
          c.relname     = ?}
    );

    $uniq_sth->execute($table->schema, $table->name);
    while(my $row = $uniq_sth->fetchrow_arrayref) {
        my ($tableid, $indexname, $col_nums) = @$row;
        $col_nums =~ s/^\s+//;
        my @col_nums = split(/\s+/, $col_nums);
        my @col_names;

        foreach (@col_nums) {
            $attr_sth->execute($tableid, $_);
            my $name_aref = $attr_sth->fetchrow_arrayref;
            push(@col_names, $self->_lc($name_aref->[0])) if $name_aref;
        }

        if(!@col_names) {
            warn "Failed to parse UNIQUE constraint $indexname on $table";
        }
        else {
            push(@uniqs, [ $indexname => \@col_names ]);
        }
    }

    return \@uniqs;
}

sub _table_comment {
    my $self = shift;
    my ($table) = @_;

    my $table_comment = $self->next::method(@_);

    return $table_comment if $table_comment;

    ($table_comment) = $self->dbh->selectrow_array(<<'EOF', {}, $table->name, $table->schema);
SELECT obj_description(oid)
FROM pg_class
WHERE relname=? AND relnamespace=(SELECT oid FROM pg_namespace WHERE nspname=?)
EOF

    return $table_comment
}


sub _column_comment {
    my $self = shift;
    my ($table, $column_number, $column_name) = @_;

    my $column_comment = $self->next::method(@_);

    return $column_comment if $column_comment;

    my ($table_oid) = $self->dbh->selectrow_array(<<'EOF', {}, $table->name, $table->schema);
SELECT oid
FROM pg_class
WHERE relname=? AND relnamespace=(SELECT oid FROM pg_namespace WHERE nspname=?)
EOF

    return $self->dbh->selectrow_array('SELECT col_description(?,?)', {}, $table_oid, $column_number);
}

# Make sure data_type's that don't need it don't have a 'size' column_info, and
# set the correct precision for datetime and varbit types.
sub _columns_info_for {
    my $self = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        my $data_type = $info->{data_type};

        # these types are fixed size
        # XXX should this be a negative match?
        if ($data_type =~
/^(?:bigint|int8|bigserial|serial8|bool(?:ean)?|box|bytea|cidr|circle|date|double precision|float8|inet|integer|int|int4|line|lseg|macaddr|money|path|point|polygon|real|float4|smallint|int2|serial|serial4|text)\z/i) {
            delete $info->{size};
        }
# for datetime types, check if it has a precision or not
        elsif ($data_type =~ /^(?:interval|time|timestamp)\b/i) {
            if (lc($data_type) eq 'timestamp without time zone') {
                $info->{data_type} = 'timestamp';
            }
            elsif (lc($data_type) eq 'time without time zone') {
                $info->{data_type} = 'time';
            }

            my ($precision) = $self->schema->storage->dbh
                ->selectrow_array(<<EOF, {}, $table->name, $col);
SELECT datetime_precision
FROM information_schema.columns
WHERE table_name = ? and column_name = ?
EOF

            if ($data_type =~ /^time\b/i) {
                if ((not $precision) || $precision !~ /^\d/) {
                    delete $info->{size};
                }
                else {
                    my ($integer_datetimes) = $self->dbh
                        ->selectrow_array('show integer_datetimes');

                    my $max_precision =
                        $integer_datetimes =~ /^on\z/i ? 6 : 10;

                    if ($precision == $max_precision) {
                        delete $info->{size};
                    }
                    else {
                        $info->{size} = $precision;
                    }
                }
            }
            elsif ((not $precision) || $precision !~ /^\d/ || $precision == 6) {
                delete $info->{size};
            }
            else {
                $info->{size} = $precision;
            }
        }
        elsif ($data_type =~ /^(?:bit(?: varying)?|varbit)\z/i) {
            $info->{data_type} = 'varbit' if $data_type =~ /var/i;

            my ($precision) = $self->dbh->selectrow_array(<<EOF, {}, $table->name, $col);
SELECT character_maximum_length
FROM information_schema.columns
WHERE table_name = ? and column_name = ?
EOF

            $info->{size} = $precision if $precision;

            $info->{size} = 1 if (not $precision) && lc($data_type) eq 'bit';
        }
        elsif ($data_type =~ /^(?:numeric|decimal)\z/i && (my $size = $info->{size})) {
            $size =~ s/\s*//g;

            my ($scale, $precision) = split /,/, $size;

            $info->{size} = [ $precision, $scale ];
        }
        elsif (lc($data_type) eq 'character varying') {
            $info->{data_type} = 'varchar';

            if (not $info->{size}) {
                $info->{data_type}           = 'text';
                $info->{original}{data_type} = 'varchar';
            }
        }
        elsif (lc($data_type) eq 'character') {
            $info->{data_type} = 'char';
        }
        else {
            my ($typetype) = $self->schema->storage->dbh
                ->selectrow_array(<<EOF, {}, $data_type);
SELECT typtype
FROM pg_catalog.pg_type
WHERE typname = ?
EOF
            if ($typetype && $typetype eq 'e') {
                # The following will extract a list of allowed values for the
                # enum.
                my $typevalues = $self->dbh
                    ->selectall_arrayref(<<EOF, {}, $info->{data_type});
SELECT e.enumlabel
FROM pg_catalog.pg_enum e
JOIN pg_catalog.pg_type t ON t.oid = e.enumtypid
WHERE t.typname = ?
EOF

                $info->{extra}{list} = [ map { $_->[0] } @$typevalues ];

                # Store its original name in extra for SQLT to pick up.
                $info->{extra}{custom_type_name} = $info->{data_type};

                $info->{data_type} = 'enum';

                delete $info->{size};
            }
        }

# process SERIAL columns
        if (ref($info->{default_value}) eq 'SCALAR'
                && ${ $info->{default_value} } =~ /\bnextval\('([^:]+)'/i) {
            $info->{is_auto_increment} = 1;
            $info->{sequence}          = $1;
            delete $info->{default_value};
        }

# alias now() to current_timestamp for deploying to other DBs
        if ((eval { lc ${ $info->{default_value} } }||'') eq 'now()') {
            # do not use a ref to a constant, that breaks Data::Dump output
            ${$info->{default_value}} = 'current_timestamp';

            my $now = 'now()';
            $info->{original}{default_value} = \$now;
        }

# detect 0/1 for booleans and rewrite
        if ($data_type =~ /^bool/i && exists $info->{default_value}) {
            if ($info->{default_value} eq '0') {
                my $false = 'false';
                $info->{default_value} = \$false;
            }
            elsif ($info->{default_value} eq '1') {
                my $true = 'true';
                $info->{default_value} = \$true;
            }
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
