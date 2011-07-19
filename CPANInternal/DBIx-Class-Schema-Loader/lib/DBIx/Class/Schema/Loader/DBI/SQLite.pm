package DBIx::Class::Schema::Loader::DBI::SQLite;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault
    DBIx::Class::Schema::Loader::DBI
/;
use Carp::Clan qw/^DBIx::Class/;
use Text::Balanced qw( extract_bracketed );
use Class::C3;

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::SQLite - DBIx::Class::Schema::Loader::DBI SQLite Implementation.

=head1 SYNOPSIS

  package My::Schema;
  use base qw/DBIx::Class::Schema::Loader/;

  __PACKAGE__->loader_options( debug => 1 );

  1;

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

=head1 METHODS

=head2 rescan

SQLite will fail all further commands on a connection if the
underlying schema has been modified.  Therefore, any runtime
changes requiring C<rescan> also require us to re-connect
to the database.  The C<rescan> method here handles that
reconnection for you, but beware that this must occur for
any other open sqlite connections as well.

=cut

sub rescan {
    my ($self, $schema) = @_;

    $schema->storage->disconnect if $schema->storage;
    $self->next::method($schema);
}

# XXX this really needs a re-factor
sub _sqlite_parse_table {
    my ($self, $table) = @_;

    my @rels;
    my @uniqs;
    my %auto_inc;

    my $dbh = $self->schema->storage->dbh;
    my $sth = $self->{_cache}->{sqlite_master}
        ||= $dbh->prepare(q{SELECT sql FROM sqlite_master WHERE tbl_name = ?});

    $sth->execute($table);
    my ($sql) = $sth->fetchrow_array;
    $sth->finish;

    # Cut "CREATE TABLE ( )" blabla...
    $sql =~ /^[\w\s"]+\((.*)\)$/si;
    my $cols = $1;

    # strip single-line comments
    $cols =~ s/\-\-.*\n/\n/g;

    # temporarily replace any commas inside parens,
    # so we don't incorrectly split on them below
    my $cols_no_bracketed_commas = $cols;
    while ( my $extracted =
        ( extract_bracketed( $cols, "()", "[^(]*" ) )[0] )
    {
        my $replacement = $extracted;
        $replacement              =~ s/,/--comma--/g;
        $replacement              =~ s/^\(//;
        $replacement              =~ s/\)$//;
        $cols_no_bracketed_commas =~ s/$extracted/$replacement/m;
    }

    # Split column definitions
    for my $col ( split /,/, $cols_no_bracketed_commas ) {

        # put the paren-bracketed commas back, to help
        # find multi-col fks below
        $col =~ s/\-\-comma\-\-/,/g;

        $col =~ s/^\s*FOREIGN\s+KEY\s*//i;

        # Strip punctuations around key and table names
        $col =~ s/[\[\]'"]/ /g;
        $col =~ s/^\s+//gs;

        # Grab reference
        chomp $col;

        if($col =~ /^(.*)\s+UNIQUE/i) {
            my $colname = $1;
            $colname =~ s/\s+.*$//;
            push(@uniqs, [ "${colname}_unique" => [ lc $colname ] ]);
        }
        elsif($col =~/^\s*UNIQUE\s*\(\s*(.*)\)/i) {
            my $cols = $1;
            $cols =~ s/\s+$//;
            my @cols = map { lc } split(/\s*,\s*/, $cols);
            my $name = join(q{_}, @cols) . '_unique';
            push(@uniqs, [ $name => \@cols ]);
        }

        if ($col =~ /AUTOINCREMENT/i) {
            $col =~ /^(\S+)/;
            $auto_inc{lc $1} = 1;
        }

        next if $col !~ /^(.*\S)\s+REFERENCES\s+(\w+) (?: \s* \( (.*) \) )? /six;

        my ($cols, $f_table, $f_cols) = ($1, $2, $3);

        if($cols =~ /^\(/) { # Table-level
            $cols =~ s/^\(\s*//;
            $cols =~ s/\s*\)$//;
        }
        else {               # Inline
            $cols =~ s/\s+.*$//s;
        }

        my @cols = map { s/\s*//g; lc $_ } split(/\s*,\s*/,$cols);
        my $rcols;
        if($f_cols) {
            my @f_cols = map { s/\s*//g; lc $_ } split(/\s*,\s*/,$f_cols);
            croak "Mismatched column count in rel for $table => $f_table"
              if @cols != @f_cols;
            $rcols = \@f_cols;
        }
        push(@rels, {
            local_columns => \@cols,
            remote_columns => $rcols,
            remote_table => $f_table,
        });
    }

    return { rels => \@rels, uniqs => \@uniqs, auto_inc => \%auto_inc };
}

sub _extra_column_info {
    my ($self, $table, $col_name, $sth, $col_num) = @_;
    ($table, $col_name) = @{$table}{qw/TABLE_NAME COLUMN_NAME/} if ref $table;
    my %extra_info;

    $self->{_sqlite_parse_data}->{$table} ||=
        $self->_sqlite_parse_table($table);

    if ($self->{_sqlite_parse_data}->{$table}->{auto_inc}->{$col_name}) {
        $extra_info{is_auto_increment} = 1;
    }

    return \%extra_info;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    $self->{_sqlite_parse_data}->{$table} ||=
        $self->_sqlite_parse_table($table);

    return $self->{_sqlite_parse_data}->{$table}->{rels};
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    $self->{_sqlite_parse_data}->{$table} ||=
        $self->_sqlite_parse_table($table);

    return $self->{_sqlite_parse_data}->{$table}->{uniqs};
}

sub _tables_list {
    my $self = shift;

    my $dbh = $self->schema->storage->dbh;
    my $sth = $dbh->prepare("SELECT * FROM sqlite_master");
    $sth->execute;
    my @tables;
    while ( my $row = $sth->fetchrow_hashref ) {
        next unless lc( $row->{type} ) eq 'table';
        next if $row->{tbl_name} =~ /^sqlite_/;
        push @tables, $row->{tbl_name};
    }
    $sth->finish;
    return @tables;
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
