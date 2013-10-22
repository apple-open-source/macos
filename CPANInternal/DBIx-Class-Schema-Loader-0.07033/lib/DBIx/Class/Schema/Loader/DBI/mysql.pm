package DBIx::Class::Schema::Loader::DBI::mysql;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use mro 'c3';
use Carp::Clan qw/^DBIx::Class/;
use List::Util 'first';
use List::MoreUtils 'any';
use Try::Tiny;
use Scalar::Util 'blessed';
use namespace::clean;
use DBIx::Class::Schema::Loader::Table ();

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::mysql - DBIx::Class::Schema::Loader::DBI mysql Implementation.

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _setup {
    my $self = shift;

    $self->schema->storage->sql_maker->quote_char("`");
    $self->schema->storage->sql_maker->name_sep(".");

    $self->next::method(@_);

    if (not defined $self->preserve_case) {
        $self->preserve_case(0);
    }

    if ($self->db_schema && $self->db_schema->[0] eq '%') {
        my @schemas = try {
            $self->_show_databases;
        }
        catch {
            croak "no SHOW DATABASES privileges: $_";
        };

        @schemas = grep {
            my $schema = $_;
            not any { lc($schema) eq lc($_) } $self->_system_schemas
        } @schemas;

        $self->db_schema(\@schemas);
    }
}

sub _show_databases {
    my $self = shift;

    return map $_->[0], @{ $self->dbh->selectall_arrayref('SHOW DATABASES') };
}

sub _system_schemas {
    my $self = shift;

    return ($self->next::method(@_), 'mysql');
}

sub _tables_list {
    my ($self, $opts) = @_;

    return $self->next::method($opts, undef, undef);
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $table_def_ref = eval { $self->dbh->selectrow_arrayref("SHOW CREATE TABLE ".$table->sql_name) };
    my $table_def = $table_def_ref->[1];

    return [] if not $table_def;

    my $qt  = qr/["`]/;
    my $nqt = qr/[^"`]/;

    my (@reldata) = ($table_def =~
        /CONSTRAINT ${qt}${nqt}+${qt} FOREIGN KEY \($qt(.*)$qt\) REFERENCES (?:$qt($nqt+)$qt\.)?$qt($nqt+)$qt \($qt(.+)$qt\)\s*(.*)/ig
    );

    my @rels;
    while (scalar @reldata > 0) {
        my ($cols, $f_schema, $f_table, $f_cols, $rest) = splice @reldata, 0, 5;

        my @cols   = map { s/$qt//g; $self->_lc($_) }
            split(/$qt?\s*$qt?,$qt?\s*$qt?/, $cols);

        my @f_cols = map { s/$qt//g; $self->_lc($_) }
            split(/$qt?\s*$qt?,$qt?\s*$qt?/, $f_cols);

        # Match case of remote schema to that in SHOW DATABASES, if it's there
        # and we have permissions to run SHOW DATABASES.
        if ($f_schema) {
            my $matched = first {
                lc($_) eq lc($f_schema)
            } try { $self->_show_databases };

            $f_schema = $matched if $matched;
        }

        my $remote_table = do {
            # Get ->tables_list to return tables from the remote schema, in case it is not in the db_schema list.
            local $self->{db_schema} = [ $f_schema ] if $f_schema;

            first {
                   lc($_->name) eq lc($f_table)
                && ((not $f_schema) || lc($_->schema) eq lc($f_schema))
            } $self->_tables_list;
        };

        # The table may not be in any database, or it may not have been found by the previous code block for whatever reason.
        if (not $remote_table) {
            my $remote_schema = $f_schema || $self->db_schema && @{ $self->db_schema } == 1 && $self->db_schema->[0];

            $remote_table = DBIx::Class::Schema::Loader::Table->new(
                loader => $self,
                name   => $f_table,
                ($remote_schema ? (
                    schema => $remote_schema,
                ) : ()),
            );
        }

        my %attrs;

        if ($rest) {
            my @on_clauses = $rest =~ /(ON DELETE|ON UPDATE) (RESTRICT|CASCADE|SET NULL|NO ACTION) ?/ig;

            while (my ($clause, $value) = splice @on_clauses, 0, 2) {
                $clause = lc $clause;
                $clause =~ s/ /_/;

                $value = uc $value;

                $attrs{$clause} = $value;
            }
        }

# The default behavior is RESTRICT. Specifying RESTRICT explicitly just removes
# that ON clause from the SHOW CREATE TABLE output. For this reason, even
# though the default for these clauses everywhere else in Schema::Loader is
# CASCADE, we change the default here to RESTRICT in order to reproduce the
# schema faithfully.
        $attrs{on_delete}     ||= 'RESTRICT';
        $attrs{on_update}     ||= 'RESTRICT';

# MySQL does not have a DEFERRABLE attribute, but there is a way to defer FKs.
        $attrs{is_deferrable}   = 1;

        push(@rels, {
            local_columns => \@cols,
            remote_columns => \@f_cols,
            remote_table => $remote_table,
            attrs => \%attrs,
        });
    }

    return \@rels;
}

# primary and unique info comes from the same sql statement,
#   so cache it here for both routines to use
sub _mysql_table_get_keys {
    my ($self, $table) = @_;

    if(!exists($self->{_cache}->{_mysql_keys}->{$table->sql_name})) {
        my %keydata;
        my $sth = $self->dbh->prepare('SHOW INDEX FROM '.$table->sql_name);
        $sth->execute;
        while(my $row = $sth->fetchrow_hashref) {
            next if $row->{Non_unique};
            push(@{$keydata{$row->{Key_name}}},
                [ $row->{Seq_in_index}, $self->_lc($row->{Column_name}) ]
            );
        }
        foreach my $keyname (keys %keydata) {
            my @ordered_cols = map { $_->[1] } sort { $a->[0] <=> $b->[0] }
                @{$keydata{$keyname}};
            $keydata{$keyname} = \@ordered_cols;
        }
        $self->{_cache}->{_mysql_keys}->{$table->sql_name} = \%keydata;
    }

    return $self->{_cache}->{_mysql_keys}->{$table->sql_name};
}

sub _table_pk_info {
    my ( $self, $table ) = @_;

    return $self->_mysql_table_get_keys($table)->{PRIMARY};
}

sub _table_uniq_info {
    my ( $self, $table ) = @_;

    my @uniqs;
    my $keydata = $self->_mysql_table_get_keys($table);
    foreach my $keyname (keys %$keydata) {
        next if $keyname eq 'PRIMARY';
        push(@uniqs, [ $keyname => $keydata->{$keyname} ]);
    }

    return \@uniqs;
}

sub _columns_info_for {
    my $self = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        if ($info->{data_type} eq 'int') {
            $info->{data_type} = 'integer';
        }
        elsif ($info->{data_type} eq 'double') {
            $info->{data_type} = 'double precision';
        }
        my $data_type = $info->{data_type};

        delete $info->{size} if $data_type !~ /^(?: (?:var)?(?:char(?:acter)?|binary) | bit | year)\z/ix;

        # information_schema is available in 5.0+
        my ($precision, $scale, $column_type, $default) = eval { $self->dbh->selectrow_array(<<'EOF', {}, $table->name, lc($col)) };
SELECT numeric_precision, numeric_scale, column_type, column_default
FROM information_schema.columns
WHERE table_name = ? AND lower(column_name) = ?
EOF
        my $has_information_schema = not $@;

        $column_type = '' if not defined $column_type;

        if ($data_type eq 'bit' && (not exists $info->{size})) {
            $info->{size} = $precision if defined $precision;
        }
        elsif ($data_type =~ /^(?:float|double precision|decimal)\z/i) {
            if (defined $precision && defined $scale) {
                if ($precision == 10 && $scale == 0) {
                    delete $info->{size};
                }
                else {
                    $info->{size} = [$precision,$scale];
                }
            }
        }
        elsif ($data_type eq 'year') {
            if ($column_type =~ /\(2\)/) {
                $info->{size} = 2;
            }
            elsif ($column_type =~ /\(4\)/ || $info->{size} == 4) {
                delete $info->{size};
            }
        }
        elsif ($data_type =~ /^(?:date(?:time)?|timestamp)\z/) {
            if (not (defined $self->datetime_undef_if_invalid && $self->datetime_undef_if_invalid == 0)) {
                $info->{datetime_undef_if_invalid} = 1;
            }
        }
        elsif ($data_type =~ /^(?:enum|set)\z/ && $has_information_schema
               && $column_type =~ /^(?:enum|set)\(/) {

            delete $info->{extra}{list};

            while ($column_type =~ /'((?:[^']* (?:''|\\')* [^']*)* [^\\'])',?/xg) {
                my $el = $1;
                $el =~ s/''/'/g;
                push @{ $info->{extra}{list} }, $el;
            }
        }

        # Sometimes apparently there's a bug where default_value gets set to ''
        # for things that don't actually have or support that default (like ints.)
        if (exists $info->{default_value} && $info->{default_value} eq '') {
            if ($has_information_schema) {
                if (not defined $default) {
                    delete $info->{default_value};
                }
            }
            else { # just check if it's a char/text type, otherwise remove
                delete $info->{default_value} unless $data_type =~ /char|text/i;
            }
        }
    }

    return $result;
}

sub _extra_column_info {
    no warnings 'uninitialized';
    my ($self, $table, $col, $info, $dbi_info) = @_;
    my %extra_info;

    if ($dbi_info->{mysql_is_auto_increment}) {
        $extra_info{is_auto_increment} = 1
    }
    if ($dbi_info->{mysql_type_name} =~ /\bunsigned\b/i) {
        $extra_info{extra}{unsigned} = 1;
    }
    if ($dbi_info->{mysql_values}) {
        $extra_info{extra}{list} = $dbi_info->{mysql_values};
    }
    if ((not blessed $dbi_info) # isa $sth
        && lc($dbi_info->{COLUMN_DEF})      eq 'current_timestamp'
        && lc($dbi_info->{mysql_type_name}) eq 'timestamp') {

        my $current_timestamp = 'current_timestamp';
        $extra_info{default_value} = \$current_timestamp;
    }

    return \%extra_info;
}

sub _dbh_column_info {
    my $self = shift;

    local $SIG{__WARN__} = sub { warn @_
        unless $_[0] =~ /^column_info: unrecognized column type/ };

    $self->next::method(@_);
}

sub _table_comment {
    my ( $self, $table ) = @_;
    my $comment = $self->next::method($table);
    if (not $comment) {
        ($comment) = try { $self->schema->storage->dbh->selectrow_array(
            qq{SELECT table_comment
                FROM information_schema.tables
                WHERE table_schema = schema()
                  AND table_name = ?
            }, undef, $table->name);
        };
        # InnoDB likes to auto-append crap.
        if (not $comment) {
            # Do nothing.
        }
        elsif ($comment =~ /^InnoDB free:/) {
            $comment = undef;
        }
        else {
            $comment =~ s/; InnoDB.*//;
        }
    }
    return $comment;
}

sub _column_comment {
    my ( $self, $table, $column_number, $column_name ) = @_;
    my $comment = $self->next::method($table, $column_number, $column_name);
    if (not $comment) {
        ($comment) = try { $self->schema->storage->dbh->selectrow_array(
            qq{SELECT column_comment
                FROM information_schema.columns
                WHERE table_schema = schema()
                  AND table_name = ?
                  AND lower(column_name) = ?
            }, undef, $table->name, lc($column_name));
        };
    }
    return $comment;
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
