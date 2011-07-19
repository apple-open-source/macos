package # hide from PAUSE 
    DBICTest;

use strict;
use warnings;
use DBICTest::AuthorCheck;
use DBICTest::Schema;

=head1 NAME

DBICTest - Library to be used by DBIx::Class test scripts.

=head1 SYNOPSIS

  use lib qw(t/lib);
  use DBICTest;
  use Test::More;
  
  my $schema = DBICTest->init_schema();

=head1 DESCRIPTION

This module provides the basic utilities to write tests against 
DBIx::Class.

=head1 METHODS

=head2 init_schema

  my $schema = DBICTest->init_schema(
    no_deploy=>1,
    no_populate=>1,
    storage_type=>'::DBI::Replicated',
    storage_type_args=>{
      balancer_type=>'DBIx::Class::Storage::DBI::Replicated::Balancer::Random'
    },
  );

This method removes the test SQLite database in t/var/DBIxClass.db 
and then creates a new, empty database.

This method will call deploy_schema() by default, unless the 
no_deploy flag is set.

Also, by default, this method will call populate_schema() by 
default, unless the no_deploy or no_populate flags are set.

=cut

sub has_custom_dsn {
    return $ENV{"DBICTEST_DSN"} ? 1:0;
}

sub _sqlite_dbfilename {
    return "t/var/DBIxClass.db";
}

sub _sqlite_dbname {
    my $self = shift;
    my %args = @_;
    return $self->_sqlite_dbfilename if $args{sqlite_use_file} or $ENV{"DBICTEST_SQLITE_USE_FILE"};
    return ":memory:";
}

sub _database {
    my $self = shift;
    my %args = @_;
    my $db_file = $self->_sqlite_dbname(%args);

    unlink($db_file) if -e $db_file;
    unlink($db_file . "-journal") if -e $db_file . "-journal";
    mkdir("t/var") unless -d "t/var";

    my $dsn = $ENV{"DBICTEST_DSN"} || "dbi:SQLite:${db_file}";
    my $dbuser = $ENV{"DBICTEST_DBUSER"} || '';
    my $dbpass = $ENV{"DBICTEST_DBPASS"} || '';

    my @connect_info = ($dsn, $dbuser, $dbpass, { AutoCommit => 1, %args });

    return @connect_info;
}

sub init_schema {
    my $self = shift;
    my %args = @_;

    my $schema;

    if ($args{compose_connection}) {
      $schema = DBICTest::Schema->compose_connection(
                  'DBICTest', $self->_database(%args)
                );
    } else {
      $schema = DBICTest::Schema->compose_namespace('DBICTest');
    }
    if( $args{storage_type}) {
      $schema->storage_type($args{storage_type});
    }
    if ( !$args{no_connect} ) {
      $schema = $schema->connect($self->_database(%args));
      $schema->storage->on_connect_do(['PRAGMA synchronous = OFF'])
       unless $self->has_custom_dsn;
    }
    if ( !$args{no_deploy} ) {
        __PACKAGE__->deploy_schema( $schema, $args{deploy_args} );
        __PACKAGE__->populate_schema( $schema )
         if( !$args{no_populate} );
    }
    return $schema;
}

=head2 deploy_schema

  DBICTest->deploy_schema( $schema );

This method does one of two things to the schema.  It can either call 
the experimental $schema->deploy() if the DBICTEST_SQLT_DEPLOY environment 
variable is set, otherwise the default is to read in the t/lib/sqlite.sql 
file and execute the SQL within. Either way you end up with a fresh set 
of tables for testing.

=cut

sub deploy_schema {
    my $self = shift;
    my $schema = shift;
    my $args = shift || {};

    if ($ENV{"DBICTEST_SQLT_DEPLOY"}) { 
        $schema->deploy($args);
    } else {
        open IN, "t/lib/sqlite.sql";
        my $sql;
        { local $/ = undef; $sql = <IN>; }
        close IN;
        for my $chunk ( split (/;\s*\n+/, $sql) ) {
          if ( $chunk =~ / ^ (?! --\s* ) \S /xm ) {  # there is some real sql in the chunk - a non-space at the start of the string which is not a comment
            $schema->storage->dbh_do(sub { $_[1]->do($chunk) }) or print "Error on SQL: $chunk\n";
          }
        }
    }
    return;
}

=head2 populate_schema

  DBICTest->populate_schema( $schema );

After you deploy your schema you can use this method to populate 
the tables with test data.

=cut

sub populate_schema {
    my $self = shift;
    my $schema = shift;

    $schema->populate('Genre', [
      [qw/genreid name/],
      [qw/1       emo  /],
    ]);

    $schema->populate('Artist', [
        [ qw/artistid name/ ],
        [ 1, 'Caterwauler McCrae' ],
        [ 2, 'Random Boy Band' ],
        [ 3, 'We Are Goth' ],
    ]);

    $schema->populate('CD', [
        [ qw/cdid artist title year genreid/ ],
        [ 1, 1, "Spoonful of bees", 1999, 1 ],
        [ 2, 1, "Forkful of bees", 2001 ],
        [ 3, 1, "Caterwaulin' Blues", 1997 ],
        [ 4, 2, "Generic Manufactured Singles", 2001 ],
        [ 5, 3, "Come Be Depressed With Us", 1998 ],
    ]);

    $schema->populate('LinerNotes', [
        [ qw/liner_id notes/ ],
        [ 2, "Buy Whiskey!" ],
        [ 4, "Buy Merch!" ],
        [ 5, "Kill Yourself!" ],
    ]);

    $schema->populate('Tag', [
        [ qw/tagid cd tag/ ],
        [ 1, 1, "Blue" ],
        [ 2, 2, "Blue" ],
        [ 3, 3, "Blue" ],
        [ 4, 5, "Blue" ],
        [ 5, 2, "Cheesy" ],
        [ 6, 4, "Cheesy" ],
        [ 7, 5, "Cheesy" ],
        [ 8, 2, "Shiny" ],
        [ 9, 4, "Shiny" ],
    ]);

    $schema->populate('TwoKeys', [
        [ qw/artist cd/ ],
        [ 1, 1 ],
        [ 1, 2 ],
        [ 2, 2 ],
    ]);

    $schema->populate('FourKeys', [
        [ qw/foo bar hello goodbye sensors/ ],
        [ 1, 2, 3, 4, 'online' ],
        [ 5, 4, 3, 6, 'offline' ],
    ]);

    $schema->populate('OneKey', [
        [ qw/id artist cd/ ],
        [ 1, 1, 1 ],
        [ 2, 1, 2 ],
        [ 3, 2, 2 ],
    ]);

    $schema->populate('SelfRef', [
        [ qw/id name/ ],
        [ 1, 'First' ],
        [ 2, 'Second' ],
    ]);

    $schema->populate('SelfRefAlias', [
        [ qw/self_ref alias/ ],
        [ 1, 2 ]
    ]);

    $schema->populate('ArtistUndirectedMap', [
        [ qw/id1 id2/ ],
        [ 1, 2 ]
    ]);

    $schema->populate('Producer', [
        [ qw/producerid name/ ],
        [ 1, 'Matt S Trout' ],
        [ 2, 'Bob The Builder' ],
        [ 3, 'Fred The Phenotype' ],
    ]);

    $schema->populate('CD_to_Producer', [
        [ qw/cd producer/ ],
        [ 1, 1 ],
        [ 1, 2 ],
        [ 1, 3 ],
    ]);
    
    $schema->populate('TreeLike', [
        [ qw/id parent name/ ],
        [ 1, undef, 'root' ],
        [ 2, 1, 'foo'  ],
        [ 3, 2, 'bar'  ],
        [ 6, 2, 'blop' ],
        [ 4, 3, 'baz'  ],
        [ 5, 4, 'quux' ],
        [ 7, 3, 'fong'  ],
    ]);

    $schema->populate('Track', [
        [ qw/trackid cd  position title/ ],
        [ 4, 2, 1, "Stung with Success"],
        [ 5, 2, 2, "Stripy"],
        [ 6, 2, 3, "Sticky Honey"],
        [ 7, 3, 1, "Yowlin"],
        [ 8, 3, 2, "Howlin"],
        [ 9, 3, 3, "Fowlin"],
        [ 10, 4, 1, "Boring Name"],
        [ 11, 4, 2, "Boring Song"],
        [ 12, 4, 3, "No More Ideas"],
        [ 13, 5, 1, "Sad"],
        [ 14, 5, 2, "Under The Weather"],
        [ 15, 5, 3, "Suicidal"],
        [ 16, 1, 1, "The Bees Knees"],
        [ 17, 1, 2, "Apiary"],
        [ 18, 1, 3, "Beehind You"],
    ]);

    $schema->populate('Event', [
        [ qw/id starts_at created_on varchar_date varchar_datetime skip_inflation/ ],
        [ 1, '2006-04-25 22:24:33', '2006-06-22 21:00:05', '2006-07-23', '2006-05-22 19:05:07', '2006-04-21 18:04:06'],
    ]);

    $schema->populate('Link', [
        [ qw/id url title/ ],
        [ 1, '', 'aaa' ]
    ]);

    $schema->populate('Bookmark', [
        [ qw/id link/ ],
        [ 1, 1 ]
    ]);

    $schema->populate('Collection', [
        [ qw/collectionid name/ ],
        [ 1, "Tools" ],
        [ 2, "Body Parts" ],
    ]);
    
    $schema->populate('TypedObject', [
        [ qw/objectid type value/ ],
        [ 1, "pointy", "Awl" ],
        [ 2, "round", "Bearing" ],
        [ 3, "pointy", "Knife" ],
        [ 4, "pointy", "Tooth" ],
        [ 5, "round", "Head" ],
    ]);
    $schema->populate('CollectionObject', [
        [ qw/collection object/ ],
        [ 1, 1 ],
        [ 1, 2 ],
        [ 1, 3 ],
        [ 2, 4 ],
        [ 2, 5 ],
    ]);

    $schema->populate('Owners', [
        [ qw/id name/ ],
        [ 1, "Newton" ],
        [ 2, "Waltham" ],
    ]);

    $schema->populate('BooksInLibrary', [
        [ qw/id owner title source price/ ],
        [ 1, 1, "Programming Perl", "Library", 23 ],
        [ 2, 1, "Dynamical Systems", "Library",  37 ],
        [ 3, 2, "Best Recipe Cookbook", "Library", 65 ],
    ]);
}

1;
