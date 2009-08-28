package DBIx::Class::Storage::DBI::Replication;

use strict;
use warnings;

use DBIx::Class::Storage::DBI;
use DBD::Multi;
use base qw/Class::Accessor::Fast/;

__PACKAGE__->mk_accessors( qw/read_source write_source/ );

=head1 NAME

DBIx::Class::Storage::DBI::Replication - EXPERIMENTAL Replicated database support

=head1 SYNOPSIS

  # change storage_type in your schema class
    $schema->storage_type( '::DBI::Replication' );
    $schema->connect_info( [
		     [ "dbi:mysql:database=test;hostname=master", "username", "password", { AutoCommit => 1 } ], # master
		     [ "dbi:mysql:database=test;hostname=slave1", "username", "password", { priority => 10 } ],  # slave1
		     [ "dbi:mysql:database=test;hostname=slave2", "username", "password", { priority => 10 } ],  # slave2
		     <...>,
		     { limit_dialect => 'LimitXY' } # If needed, see below
		    ] );

=head1 DESCRIPTION

Warning: This class is marked EXPERIMENTAL. It works for the authors but does
not currently have automated tests so your mileage may vary.

This class implements replicated data store for DBI. Currently you can define
one master and numerous slave database connections. All write-type queries
(INSERT, UPDATE, DELETE and even LAST_INSERT_ID) are routed to master
database, all read-type queries (SELECTs) go to the slave database.

For every slave database you can define a priority value, which controls data
source usage pattern. It uses L<DBD::Multi>, so first the lower priority data
sources used (if they have the same priority, the are used randomized), than
if all low priority data sources fail, higher ones tried in order.

=head1 CONFIGURATION

=head2 Limit dialect

If you use LIMIT in your queries (effectively, if you use
SQL::Abstract::Limit), do not forget to set up limit_dialect (perldoc
SQL::Abstract::Limit) by passing it as an option in the (optional) hash
reference to connect_info.  DBIC can not set it up automatically, since it can
not guess DBD::Multi connection types.

=cut

sub new {
    my $proto = shift;
    my $class = ref( $proto ) || $proto;
    my $self = {};

    bless( $self, $class );

    $self->write_source( DBIx::Class::Storage::DBI->new );
    $self->read_source( DBIx::Class::Storage::DBI->new );

    return $self;
}

sub all_sources {
    my $self = shift;

    my @sources = ($self->read_source, $self->write_source);

    return wantarray ? @sources : \@sources;
}

sub connect_info {
    my( $self, $source_info ) = @_;

    my( $info, $global_options, $options, @dsns );

    $info = [ @$source_info ];

    $global_options = ref $info->[-1] eq 'HASH' ? pop( @$info ) : {};
    if( ref( $options = $info->[0]->[-1] ) eq 'HASH' ) {
	# Local options present in dsn, merge them with global options
	map { $global_options->{$_} = $options->{$_} } keys %$options;
	pop @{$info->[0]};
    }

    # We need to copy-pass $global_options, since connect_info clears it while
    # processing options
    $self->write_source->connect_info( [ @{$info->[0]}, { %$global_options } ] );

    @dsns = map { ($_->[3]->{priority} || 10) => $_ } @{$info}[1..@$info-1];
    $global_options->{dsns} = \@dsns;

    $self->read_source->connect_info( [ 'dbi:Multi:', undef, undef, { %$global_options } ] );
}

sub select {
    shift->read_source->select( @_ );
}
sub select_single {
    shift->read_source->select_single( @_ );
}
sub throw_exception {
    shift->read_source->throw_exception( @_ );
}
sub sql_maker {
    shift->read_source->sql_maker( @_ );
}
sub columns_info_for {
    shift->read_source->columns_info_for( @_ );
}
sub sqlt_type {
    shift->read_source->sqlt_type( @_ );
}
sub create_ddl_dir {
    shift->read_source->create_ddl_dir( @_ );
}
sub deployment_statements {
    shift->read_source->deployment_statements( @_ );
}
sub datetime_parser {
    shift->read_source->datetime_parser( @_ );
}
sub datetime_parser_type {
    shift->read_source->datetime_parser_type( @_ );
}
sub build_datetime_parser {
    shift->read_source->build_datetime_parser( @_ );
}

sub limit_dialect { $_->limit_dialect( @_ ) for( shift->all_sources ) }
sub quote_char { $_->quote_char( @_ ) for( shift->all_sources ) }
sub name_sep { $_->quote_char( @_ ) for( shift->all_sources ) }
sub disconnect { $_->disconnect( @_ ) for( shift->all_sources ) }
sub set_schema { $_->set_schema( @_ ) for( shift->all_sources ) }

sub DESTROY {
    my $self = shift;

    undef $self->{write_source};
    undef $self->{read_sources};
}

sub last_insert_id {
    shift->write_source->last_insert_id( @_ );
}
sub insert {
    shift->write_source->insert( @_ );
}
sub update {
    shift->write_source->update( @_ );
}
sub update_all {
    shift->write_source->update_all( @_ );
}
sub delete {
    shift->write_source->delete( @_ );
}
sub delete_all {
    shift->write_source->delete_all( @_ );
}
sub create {
    shift->write_source->create( @_ );
}
sub find_or_create {
    shift->write_source->find_or_create( @_ );
}
sub update_or_create {
    shift->write_source->update_or_create( @_ );
}
sub connected {
    shift->write_source->connected( @_ );
}
sub ensure_connected {
    shift->write_source->ensure_connected( @_ );
}
sub dbh {
    shift->write_source->dbh( @_ );
}
sub txn_begin {
    shift->write_source->txn_begin( @_ );
}
sub txn_commit {
    shift->write_source->txn_commit( @_ );
}
sub txn_rollback {
    shift->write_source->txn_rollback( @_ );
}
sub sth {
    shift->write_source->sth( @_ );
}
sub deploy {
    shift->write_source->deploy( @_ );
}


sub debugfh { shift->_not_supported( 'debugfh' ) };
sub debugcb { shift->_not_supported( 'debugcb' ) };

sub _not_supported {
    my( $self, $method ) = @_;

    die "This Storage does not support $method method.";
}

=head1 SEE ALSO

L<DBI::Class::Storage::DBI>, L<DBD::Multi>, L<DBI>

=head1 AUTHOR

Norbert Csongrádi <bert@cpan.org>

Peter Siklósi <einon@einon.hu>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut

1;
