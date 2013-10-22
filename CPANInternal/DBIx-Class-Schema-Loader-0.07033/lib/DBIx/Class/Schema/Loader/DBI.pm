package DBIx::Class::Schema::Loader::DBI;

use strict;
use warnings;
use base qw/DBIx::Class::Schema::Loader::Base/;
use mro 'c3';
use Try::Tiny;
use List::MoreUtils 'any';
use Carp::Clan qw/^DBIx::Class/;
use namespace::clean;
use DBIx::Class::Schema::Loader::Table ();

our $VERSION = '0.07033';

__PACKAGE__->mk_group_accessors('simple', qw/
    _disable_pk_detection
    _disable_uniq_detection
    _disable_fk_detection
    _passwords
    quote_char
    name_sep
/);

=head1 NAME

DBIx::Class::Schema::Loader::DBI - DBIx::Class::Schema::Loader DBI Implementation.

=head1 SYNOPSIS

See L<DBIx::Class::Schema::Loader::Base>

=head1 DESCRIPTION

This is the base class for L<DBIx::Class::Schema::Loader::Base> classes for
DBI-based storage backends, and implements the common functionality between them.

See L<DBIx::Class::Schema::Loader::Base> for the available options.

=head1 METHODS

=head2 new

Overlays L<DBIx::Class::Schema::Loader::Base/new> to do some DBI-specific
things.

=cut

sub new {
    my $self = shift->next::method(@_);

    # rebless to vendor-specific class if it exists and loads and we're not in a
    # custom class.
    if (not $self->loader_class) {
        my $driver = $self->dbh->{Driver}->{Name};

        my $subclass = 'DBIx::Class::Schema::Loader::DBI::' . $driver;
        if ((not $self->isa($subclass)) && $self->load_optional_class($subclass)) {
            bless $self, $subclass;
            $self->_rebless;
            Class::C3::reinitialize() if $] < 5.009005;
        }
    }

    # Set up the default quoting character and name seperators
    $self->quote_char($self->_build_quote_char);
    $self->name_sep($self->_build_name_sep);

    $self->_setup;

    return $self;
}

sub _build_quote_char {
    my $self = shift;

    my $quote_char = $self->dbh->get_info(29)
           || $self->schema->storage->sql_maker->quote_char
           || q{"};

    # For our usage as regex matches, concatenating multiple quote_char
    # values works fine (e.g. s/[\Q<>\E]// if quote_char was [ '<', '>' ])
    if (ref $quote_char eq 'ARRAY') {
        $quote_char = join '', @$quote_char;
    }

    return $quote_char;
}

sub _build_name_sep {
    my $self = shift;
    return $self->dbh->get_info(41)
           || $self->schema->storage->sql_maker->name_sep
           || '.';
}

# Override this in vendor modules to do things at the end of ->new()
sub _setup { }

# Override this in vendor module to load a subclass if necessary
sub _rebless { }

sub _system_schemas {
    return ('information_schema');
}

sub _system_tables {
    return ();
}

sub _dbh_tables {
    my ($self, $schema) = (shift, shift);

    my ($table_pattern, $table_type_pattern) = @_ ? @_ : ('%', '%');

    return $self->dbh->tables(undef, $schema, $table_pattern, $table_type_pattern);
}

# default to be overridden in subclasses if necessary
sub _supports_db_schema { 1 }

# Returns an array of table objects
sub _tables_list {
    my ($self, $opts) = (shift, shift);

    my @tables;

    my $qt  = qr/[\Q$self->{quote_char}\E"'`\[\]]/;
    my $nqt = qr/[^\Q$self->{quote_char}\E"'`\[\]]/;
    my $ns  = qr/[\Q$self->{name_sep}\E]/;
    my $nns = qr/[^\Q$self->{name_sep}\E]/;

    foreach my $schema (@{ $self->db_schema || [undef] }) {
        my @raw_table_names = $self->_dbh_tables($schema, @_);

        TABLE: foreach my $raw_table_name (@raw_table_names) {
            my $quoted = $raw_table_name =~ /^$qt/;

            # These regexes are not entirely correct, but hopefully they will work
            # in most cases. RT reports welcome.
            my ($schema_name, $table_name1, $table_name2) = $quoted ?
                $raw_table_name =~ /^(?:${qt}(${nqt}+?)${qt}${ns})?(?:${qt}(.+?)${qt}|(${nns}+))\z/
                :
                $raw_table_name =~ /^(?:(${nns}+?)${ns})?(?:${qt}(.+?)${qt}|(${nns}+))\z/;

            my $table_name = $table_name1 || $table_name2;

            foreach my $system_schema ($self->_system_schemas) {
                if ($schema_name) {
                    my $matches = 0;

                    if (ref $system_schema) {
                        $matches = 1
                            if $schema_name =~ $system_schema
                                 && $schema !~ $system_schema;
                    }
                    else {
                        $matches = 1
                            if $schema_name eq $system_schema
                                && $schema  ne $system_schema;
                    }

                    next TABLE if $matches;
                }
            }

            foreach my $system_table ($self->_system_tables) {
                my $matches = 0;

                if (ref $system_table) {
                    $matches = 1 if $table_name =~ $system_table;
                }
                else {
                    $matches = 1 if $table_name eq $system_table
                }

                next TABLE if $matches;
            }

            $schema_name ||= $schema;

            my $table = DBIx::Class::Schema::Loader::Table->new(
                loader => $self,
                name   => $table_name,
                schema => $schema_name,
                ($self->_supports_db_schema ? () : (
                    ignore_schema => 1
                )),
            );

            push @tables, $table;
        }
    }

    return $self->_filter_tables(\@tables, $opts);
}

# apply constraint/exclude and ignore bad tables and views
sub _filter_tables {
    my ($self, $tables, $opts) = @_;

    my @tables = @$tables;
    my @filtered_tables;

    $opts ||= {};
    my $constraint   = $opts->{constraint};
    my $exclude      = $opts->{exclude};

    @tables = grep { /$constraint/ } @tables if defined $constraint;
    @tables = grep { ! /$exclude/  } @tables if defined $exclude;

    TABLE: for my $table (@tables) {
        try {
            local $^W = 0; # for ADO
            my $sth = $self->_sth_for($table, undef, \'1 = 0');
            $sth->execute;
            1;
        }
        catch {
            warn "Bad table or view '$table', ignoring: $_\n";
            0;
        } or next TABLE;

        push @filtered_tables, $table;
    }

    return @filtered_tables;
}

=head2 load

We override L<DBIx::Class::Schema::Loader::Base/load> here to hook in our localized settings for C<$dbh> error handling.

=cut

sub load {
    my $self = shift;

    local $self->dbh->{RaiseError} = 1;
    local $self->dbh->{PrintError} = 0;

    $self->next::method(@_);

    $self->schema->storage->disconnect unless $self->dynamic;
}

sub _sth_for {
    my ($self, $table, $fields, $where) = @_;

    my $sth = $self->dbh->prepare($self->schema->storage->sql_maker
        ->select(\$table->sql_name, $fields, $where));

    return $sth;
}

# Returns an arrayref of column names
sub _table_columns {
    my ($self, $table) = @_;

    my $sth = $self->_sth_for($table, undef, \'1 = 0');
    $sth->execute;

    my $retval = [ map $self->_lc($_), @{$sth->{NAME}} ];

    $sth->finish;

    return $retval;
}

# Returns arrayref of pk col names
sub _table_pk_info {
    my ($self, $table) = @_;

    return [] if $self->_disable_pk_detection;

    my @primary = try {
        $self->dbh->primary_key('', $table->schema, $table->name);
    }
    catch {
        warn "Cannot find primary keys for this driver: $_";
        $self->_disable_pk_detection(1);
        return ();
    };

    return [] if not @primary;

    @primary = map { $self->_lc($_) } @primary;
    s/[\Q$self->{quote_char}\E]//g for @primary;

    return \@primary;
}

# Override this for vendor-specific uniq info
sub _table_uniq_info {
    my ($self, $table) = @_;

    return [] if $self->_disable_uniq_detection;

    if (not $self->dbh->can('statistics_info')) {
        warn "No UNIQUE constraint information can be gathered for this driver";
        $self->_disable_uniq_detection(1);
        return [];
    }

    my %indices;
    my $sth = $self->dbh->statistics_info(undef, $table->schema, $table->name, 1, 1);
    while(my $row = $sth->fetchrow_hashref) {
        # skip table-level stats, conditional indexes, and any index missing
        #  critical fields
        next if $row->{TYPE} eq 'table'
            || defined $row->{FILTER_CONDITION}
            || !$row->{INDEX_NAME}
            || !defined $row->{ORDINAL_POSITION}
            || !$row->{COLUMN_NAME};

        $indices{$row->{INDEX_NAME}}[$row->{ORDINAL_POSITION}] = $self->_lc($row->{COLUMN_NAME});
    }
    $sth->finish;

    my @retval;
    foreach my $index_name (keys %indices) {
        my $index = $indices{$index_name};
        push(@retval, [ $index_name => [ @$index[1..$#$index] ] ]);
    }

    return \@retval;
}

sub _table_comment {
    my ($self, $table) = @_;
    my $dbh = $self->dbh;

    my $comments_table = $table->clone;
    $comments_table->name($self->table_comments_table);

    my ($comment) =
        (exists $self->_tables->{$comments_table->sql_name} || undef)
        && try { $dbh->selectrow_array(<<"EOF") };
SELECT comment_text
FROM @{[ $comments_table->sql_name ]}
WHERE table_name = @{[ $dbh->quote($table->name) ]}
EOF

    # Failback: try the REMARKS column on table_info
    if (!$comment && $dbh->can('table_info')) {
        my $sth = $self->_dbh_table_info( $dbh, undef, $table->schema, $table->name );
        my $info = $sth->fetchrow_hashref();
        $comment = $info->{REMARKS};
    }

    return $comment;
}

sub _column_comment {
    my ($self, $table, $column_number, $column_name) = @_;
    my $dbh = $self->dbh;

    my $comments_table = $table->clone;
    $comments_table->name($self->column_comments_table);

    my ($comment) =
        (exists $self->_tables->{$comments_table->sql_name} || undef)
        && try { $dbh->selectrow_array(<<"EOF") };
SELECT comment_text
FROM @{[ $comments_table->sql_name ]}
WHERE table_name = @{[ $dbh->quote($table->name) ]}
AND column_name = @{[ $dbh->quote($column_name) ]}
EOF

    # Failback: try the REMARKS column on column_info
    if (!$comment && $dbh->can('column_info')) {
        if (my $sth = try { $self->_dbh_column_info( $dbh, undef, $table->schema, $table->name, $column_name ) }) {
            my $info = $sth->fetchrow_hashref();
            $comment = $info->{REMARKS};
        }
    }

    return $comment;
}

# Find relationships
sub _table_fk_info {
    my ($self, $table) = @_;

    return [] if $self->_disable_fk_detection;

    my $sth = try {
        $self->dbh->foreign_key_info( '', '', '',
                                '', ($table->schema || ''), $table->name );
    }
    catch {
        warn "Cannot introspect relationships for this driver: $_";
        $self->_disable_fk_detection(1);
        return undef;
    };

    return [] if !$sth;

    my %rels;

    my @rules = (
        'CASCADE',
        'RESTRICT',
        'SET NULL',
        'NO ACTION',
        'SET DEFAULT',
    );

    my $i = 1; # for unnamed rels, which hopefully have only 1 column ...
    REL: while(my $raw_rel = $sth->fetchrow_arrayref) {
        my $uk_scm  = $raw_rel->[1];
        my $uk_tbl  = $raw_rel->[2];
        my $uk_col  = $self->_lc($raw_rel->[3]);
        my $fk_scm  = $raw_rel->[5];
        my $fk_col  = $self->_lc($raw_rel->[7]);
        my $key_seq = $raw_rel->[8] - 1;
        my $relid   = ($raw_rel->[11] || ( "__dcsld__" . $i++ ));

        my $update_rule = $raw_rel->[9];
        my $delete_rule = $raw_rel->[10];

        $update_rule = $rules[$update_rule] if defined $update_rule;
        $delete_rule = $rules[$delete_rule] if defined $delete_rule;

        my $is_deferrable = $raw_rel->[13];

        ($is_deferrable = $is_deferrable == 7 ? 0 : 1)
            if defined $is_deferrable;

        foreach my $var ($uk_scm, $uk_tbl, $uk_col, $fk_scm, $fk_col, $relid) {
            $var =~ s/[\Q$self->{quote_char}\E]//g if defined $var;
        }

        if ($self->db_schema && $self->db_schema->[0] ne '%'
            && (not any { $_ eq $uk_scm } @{ $self->db_schema })) {

            next REL;
        }

        $rels{$relid}{tbl} ||= DBIx::Class::Schema::Loader::Table->new(
            loader => $self,
            name   => $uk_tbl,
            schema => $uk_scm,
            ($self->_supports_db_schema ? () : (
                ignore_schema => 1
            )),
        );

        $rels{$relid}{attrs}{on_delete}     = $delete_rule if $delete_rule;
        $rels{$relid}{attrs}{on_update}     = $update_rule if $update_rule;
        $rels{$relid}{attrs}{is_deferrable} = $is_deferrable if defined $is_deferrable;

        # Add this data IN ORDER
        $rels{$relid}{rcols}[$key_seq] = $uk_col;
        $rels{$relid}{lcols}[$key_seq] = $fk_col;
    }
    $sth->finish;

    my @rels;
    foreach my $relid (keys %rels) {
        push(@rels, {
            remote_columns => [ grep defined, @{ $rels{$relid}{rcols} } ],
            local_columns  => [ grep defined, @{ $rels{$relid}{lcols} } ],
            remote_table   => $rels{$relid}->{tbl},
            (exists $rels{$relid}{attrs} ?
                (attrs => $rels{$relid}{attrs})
                :
                ()
            ),
            _constraint_name => $relid,
        });
    }

    return \@rels;
}

# ported in from DBIx::Class::Storage::DBI:
sub _columns_info_for {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    my %result;

    if (my $sth = try { $self->_dbh_column_info($dbh, undef, $table->schema, $table->name, '%' ) }) {
        COL_INFO: while (my $info = try { $sth->fetchrow_hashref } catch { +{} }) {
            next COL_INFO unless %$info;

            my $column_info = {};
            $column_info->{data_type}     = lc $info->{TYPE_NAME};

            my $size = $info->{COLUMN_SIZE};

            if (defined $size && defined $info->{DECIMAL_DIGITS}) {
                $column_info->{size} = [$size, $info->{DECIMAL_DIGITS}];
            }
            elsif (defined $size) {
                $column_info->{size} = $size;
            }

            $column_info->{is_nullable}   = $info->{NULLABLE} ? 1 : 0;
            $column_info->{default_value} = $info->{COLUMN_DEF} if defined $info->{COLUMN_DEF};
            my $col_name = $info->{COLUMN_NAME};
            $col_name =~ s/^\"(.*)\"$/$1/;

            my $extra_info = $self->_extra_column_info(
                $table, $col_name, $column_info, $info
            ) || {};
            $column_info = { %$column_info, %$extra_info };

            $result{$col_name} = $column_info;
        }
        $sth->finish;
    }

    my $sth = $self->_sth_for($table, undef, \'1 = 0');
    $sth->execute;

    my @columns = @{ $sth->{NAME} };

    COL: for my $i (0 .. $#columns) {
        next COL if %{ $result{ $columns[$i] }||{} };

        my $column_info = {};
        $column_info->{data_type} = lc $sth->{TYPE}[$i];

        my $size = $sth->{PRECISION}[$i];

        if (defined $size && defined $sth->{SCALE}[$i]) {
            $column_info->{size} = [$size, $sth->{SCALE}[$i]];
        }
        elsif (defined $size) {
            $column_info->{size} = $size;
        }

        $column_info->{is_nullable} = $sth->{NULLABLE}[$i] ? 1 : 0;

        if ($column_info->{data_type} =~ m/^(.*?)\((.*?)\)$/) {
            $column_info->{data_type} = $1;
            $column_info->{size}    = $2;
        }

        my $extra_info = $self->_extra_column_info($table, $columns[$i], $column_info, $sth) || {};
        $column_info = { %$column_info, %$extra_info };

        $result{ $columns[$i] } = $column_info;
    }
    $sth->finish;

    foreach my $col (keys %result) {
        my $colinfo = $result{$col};
        my $type_num = $colinfo->{data_type};
        my $type_name;
        if (defined $type_num && $type_num =~ /^-?\d+\z/ && $dbh->can('type_info')) {
            my $type_name = $self->_dbh_type_info_type_name($type_num);
            $colinfo->{data_type} = lc $type_name if $type_name;
        }
    }

    # check for instances of the same column name with different case in preserve_case=0 mode
    if (not $self->preserve_case) {
        my %lc_colnames;

        foreach my $col (keys %result) {
            push @{ $lc_colnames{lc $col} }, $col;
        }

        if (keys %lc_colnames != keys %result) {
            my @offending_colnames = map @$_, grep @$_ > 1, values %lc_colnames;

            my $offending_colnames = join ", ", map "'$_'", @offending_colnames;

            croak "columns $offending_colnames in table @{[ $table->sql_name ]} collide in preserve_case=0 mode. preserve_case=1 mode required";
        }

        # apply lowercasing
        my %lc_result;

        while (my ($col, $info) = each %result) {
            $lc_result{ $self->_lc($col) } = $info;
        }

        %result = %lc_result;
    }

    return \%result;
}

# Need to override this for the buggy Firebird ODBC driver.
sub _dbh_type_info_type_name {
    my ($self, $type_num) = @_;

    # We wrap it in a try block for MSSQL+DBD::Sybase, which can have issues.
    # TODO investigate further
    my $type_info = try { $self->dbh->type_info($type_num) };

    return $type_info ? $type_info->{TYPE_NAME} : undef;
}

# do not use this, override _columns_info_for instead
sub _extra_column_info {}

# override to mask warnings if needed
sub _dbh_table_info {
    my ($self, $dbh) = (shift, shift);

    return $dbh->table_info(@_);
}

# override to mask warnings if needed (see mysql)
sub _dbh_column_info {
    my ($self, $dbh) = (shift, shift);

    return $dbh->column_info(@_);
}

# If a coderef uses DBI->connect, this should get its connect info.
sub _try_infer_connect_info_from_coderef {
    my ($self, $code) = @_;

    my ($dsn, $user, $pass, $params);

    no warnings 'redefine';

    local *DBI::connect = sub {
        (undef, $dsn, $user, $pass, $params) = @_;
    };

    $code->();

    return ($dsn, $user, $pass, $params);
}

sub dbh {
    my $self = shift;

    return $self->schema->storage->dbh;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sts=4 sw=4 tw=0:
