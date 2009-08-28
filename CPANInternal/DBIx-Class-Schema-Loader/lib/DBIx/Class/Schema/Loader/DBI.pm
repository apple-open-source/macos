package DBIx::Class::Schema::Loader::DBI;

use strict;
use warnings;
use base qw/DBIx::Class::Schema::Loader::Base/;
use Class::C3;
use Carp::Clan qw/^DBIx::Class/;
use UNIVERSAL::require;

our $VERSION = '0.04005';

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

    # rebless to vendor-specific class if it exists and loads
    my $dbh = $self->schema->storage->dbh;
    my $driver = $dbh->{Driver}->{Name};
    my $subclass = 'DBIx::Class::Schema::Loader::DBI::' . $driver;
    $subclass->require;
    if($@ && $@ !~ /^Can't locate /) {
        croak "Failed to require $subclass: $@";
    }
    elsif(!$@) {
        bless $self, "DBIx::Class::Schema::Loader::DBI::${driver}";
    }

    # Set up the default quoting character and name seperators
    $self->{_quoter} = $self->schema->storage->sql_maker->quote_char
                    || $dbh->get_info(29)
                    || q{"};

    $self->{_namesep} = $self->schema->storage->sql_maker->name_sep
                     || $dbh->get_info(41)
                     || q{.};

    # For our usage as regex matches, concatenating multiple quoter
    # values works fine (e.g. s/\Q<>\E// if quoter was [ '<', '>' ])
    if( ref $self->{_quoter} eq 'ARRAY') {
        $self->{_quoter} = join(q{}, @{$self->{_quoter}});
    }

    $self->_setup;

    $self;
}

# Override this in vendor modules to do things at the end of ->new()
sub _setup { }

# Returns an array of table names
sub _tables_list { 
    my $self = shift;

    my $dbh = $self->schema->storage->dbh;
    my @tables = $dbh->tables(undef, $self->db_schema, '%', '%');
    s/\Q$self->{_quoter}\E//g for @tables;
    s/^.*\Q$self->{_namesep}\E// for @tables;

    return @tables;
}

=head2 load

We override L<DBIx::Class::Schema::Loader::Base/load> here to hook in our localized settings for C<$dbh> error handling.

=cut

sub load {
    my $self = shift;

    local $self->schema->storage->dbh->{RaiseError} = 1;
    local $self->schema->storage->dbh->{PrintError} = 0;
    $self->next::method(@_);
}

# Returns an arrayref of column names
sub _table_columns {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    if($self->{db_schema}) {
        $table = $self->{db_schema} . $self->{_namesep} . $table;
    }

    my $sth = $dbh->prepare($self->schema->storage->sql_maker->select($table, undef, \'1 = 0'));
    $sth->execute;
    my $retval = \@{$sth->{NAME_lc}};
    $sth->finish;

    $retval;
}

# Returns arrayref of pk col names
sub _table_pk_info { 
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    my @primary = map { lc } $dbh->primary_key('', $self->db_schema, $table);
    s/\Q$self->{_quoter}\E//g for @primary;

    return \@primary;
}

# Override this for vendor-specific uniq info
sub _table_uniq_info {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;
    if(!$dbh->can('statistics_info')) {
        warn "No UNIQUE constraint information can be gathered for this vendor";
        return [];
    }

    my %indices;
    my $sth = $dbh->statistics_info(undef, $self->db_schema, $table, 1, 1);
    while(my $row = $sth->fetchrow_hashref) {
        # skip table-level stats, conditional indexes, and any index missing
        #  critical fields
        next if $row->{TYPE} eq 'table'
            || defined $row->{FILTER_CONDITION}
            || !$row->{INDEX_NAME}
            || !defined $row->{ORDINAL_POSITION}
            || !$row->{COLUMN_NAME};

        $indices{$row->{INDEX_NAME}}->{$row->{ORDINAL_POSITION}} = $row->{COLUMN_NAME};
    }
    $sth->finish;

    my @retval;
    foreach my $index_name (keys %indices) {
        my $index = $indices{$index_name};
        push(@retval, [ $index_name => [
            map { $index->{$_} }
                sort keys %$index
        ]]);
    }

    return \@retval;
}

# Find relationships
sub _table_fk_info {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;
    my $sth = $dbh->foreign_key_info( '', $self->db_schema, '',
                                      '', $self->db_schema, $table );
    return [] if !$sth;

    my %rels;

    my $i = 1; # for unnamed rels, which hopefully have only 1 column ...
    while(my $raw_rel = $sth->fetchrow_arrayref) {
        my $uk_tbl  = $raw_rel->[2];
        my $uk_col  = lc $raw_rel->[3];
        my $fk_col  = lc $raw_rel->[7];
        my $relid   = ($raw_rel->[11] || ( "__dcsld__" . $i++ ));
        $uk_tbl =~ s/\Q$self->{_quoter}\E//g;
        $uk_col =~ s/\Q$self->{_quoter}\E//g;
        $fk_col =~ s/\Q$self->{_quoter}\E//g;
        $relid  =~ s/\Q$self->{_quoter}\E//g;
        $rels{$relid}->{tbl} = $uk_tbl;
        $rels{$relid}->{cols}->{$uk_col} = $fk_col;
    }
    $sth->finish;

    my @rels;
    foreach my $relid (keys %rels) {
        push(@rels, {
            remote_columns => [ keys   %{$rels{$relid}->{cols}} ],
            local_columns  => [ values %{$rels{$relid}->{cols}} ],
            remote_table   => $rels{$relid}->{tbl},
        });
    }

    return \@rels;
}

# ported in from DBIx::Class::Storage::DBI:
sub _columns_info_for {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    if ($dbh->can('column_info')) {
        my %result;
        eval {
            my $sth = $dbh->column_info( undef, $self->db_schema, $table, '%' );
            while ( my $info = $sth->fetchrow_hashref() ){
                my %column_info;
                $column_info{data_type}   = $info->{TYPE_NAME};
                $column_info{size}      = $info->{COLUMN_SIZE};
                $column_info{is_nullable}   = $info->{NULLABLE} ? 1 : 0;
                $column_info{default_value} = $info->{COLUMN_DEF};
                my $col_name = $info->{COLUMN_NAME};
                $col_name =~ s/^\"(.*)\"$/$1/;

                $result{$col_name} = \%column_info;
            }
            $sth->finish;
        };
      return \%result if !$@ && scalar keys %result;
    }

    if($self->db_schema) {
        $table = $self->db_schema . $self->{_namesep} . $table;
    }
    my %result;
    my $sth = $dbh->prepare($self->schema->storage->sql_maker->select($table, undef, \'1 = 0'));
    $sth->execute;
    my @columns = @{$sth->{NAME_lc}};
    for my $i ( 0 .. $#columns ){
        my %column_info;
        $column_info{data_type} = $sth->{TYPE}->[$i];
        $column_info{size} = $sth->{PRECISION}->[$i];
        $column_info{is_nullable} = $sth->{NULLABLE}->[$i] ? 1 : 0;

        if ($column_info{data_type} =~ m/^(.*?)\((.*?)\)$/) {
            $column_info{data_type} = $1;
            $column_info{size}    = $2;
        }

        $result{$columns[$i]} = \%column_info;
    }
    $sth->finish;

    foreach my $col (keys %result) {
        my $colinfo = $result{$col};
        my $type_num = $colinfo->{data_type};
        my $type_name;
        if(defined $type_num && $dbh->can('type_info')) {
            my $type_info = $dbh->type_info($type_num);
            $type_name = $type_info->{TYPE_NAME} if $type_info;
            $colinfo->{data_type} = $type_name if $type_name;
        }
    }

    return \%result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>

=cut

1;
