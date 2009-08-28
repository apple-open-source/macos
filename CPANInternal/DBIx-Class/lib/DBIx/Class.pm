package DBIx::Class;

use strict;
use warnings;

use vars qw($VERSION);
use base qw/DBIx::Class::Componentised Class::Accessor::Grouped/;
use DBIx::Class::StartupCheck;


sub mk_classdata { 
  shift->mk_classaccessor(@_);
}

sub mk_classaccessor {
  my $self = shift;
  $self->mk_group_accessors('inherited', $_[0]); 
  $self->set_inherited(@_) if @_ > 1;
}

sub component_base_class { 'DBIx::Class' }

# Always remember to do all digits for the version even if they're 0
# i.e. first release of 0.XX *must* be 0.XX000. This avoids fBSD ports
# brain damage and presumably various other packaging systems too

$VERSION = '0.08010';

sub MODIFY_CODE_ATTRIBUTES {
  my ($class,$code,@attrs) = @_;
  $class->mk_classdata('__attr_cache' => {})
    unless $class->can('__attr_cache');
  $class->__attr_cache->{$code} = [@attrs];
  return ();
}

sub _attr_cache {
  my $self = shift;
  my $cache = $self->can('__attr_cache') ? $self->__attr_cache : {};
  my $rest = eval { $self->next::method };
  return $@ ? $cache : { %$cache, %$rest };
}

1;

=head1 NAME

DBIx::Class - Extensible and flexible object <-> relational mapper.

=head1 SYNOPSIS

Create a schema class called DB/Main.pm:

  package DB::Main;
  use base qw/DBIx::Class::Schema/;

  __PACKAGE__->load_classes();

  1;

Create a table class to represent artists, who have many CDs, in DB/Main/Artist.pm:

  package DB::Main::Artist;
  use base qw/DBIx::Class/;

  __PACKAGE__->load_components(qw/PK::Auto Core/);
  __PACKAGE__->table('artist');
  __PACKAGE__->add_columns(qw/ artistid name /);
  __PACKAGE__->set_primary_key('artistid');
  __PACKAGE__->has_many(cds => 'DB::Main::CD');

  1;

A table class to represent a CD, which belongs to an artist, in DB/Main/CD.pm:

  package DB::Main::CD;
  use base qw/DBIx::Class/;

  __PACKAGE__->load_components(qw/PK::Auto Core/);
  __PACKAGE__->table('cd');
  __PACKAGE__->add_columns(qw/ cdid artist title year /);
  __PACKAGE__->set_primary_key('cdid');
  __PACKAGE__->belongs_to(artist => 'DB::Main::Artist');

  1;

Then you can use these classes in your application's code:

  # Connect to your database.
  use DB::Main;
  my $schema = DB::Main->connect($dbi_dsn, $user, $pass, \%dbi_params);

  # Query for all artists and put them in an array,
  # or retrieve them as a result set object.
  my @all_artists = $schema->resultset('Artist')->all;
  my $all_artists_rs = $schema->resultset('Artist');

  # Create a result set to search for artists.
  # This does not query the DB.
  my $johns_rs = $schema->resultset('Artist')->search(
    # Build your WHERE using an SQL::Abstract structure:
    { name => { like => 'John%' } }
  );

  # Execute a joined query to get the cds.
  my @all_john_cds = $johns_rs->search_related('cds')->all;

  # Fetch only the next row.
  my $first_john = $johns_rs->next;

  # Specify ORDER BY on the query.
  my $first_john_cds_by_title_rs = $first_john->cds(
    undef,
    { order_by => 'title' }
  );

  # Create a result set that will fetch the artist relationship
  # at the same time as it fetches CDs, using only one query.
  my $millennium_cds_rs = $schema->resultset('CD')->search(
    { year => 2000 },
    { prefetch => 'artist' }
  );

  my $cd = $millennium_cds_rs->next; # SELECT ... FROM cds JOIN artists ...
  my $cd_artist_name = $cd->artist->name; # Already has the data so no query

  # new() makes a DBIx::Class::Row object but doesnt insert it into the DB.
  # create() is the same as new() then insert().
  my $new_cd = $schema->resultset('CD')->new({ title => 'Spoon' });
  $new_cd->artist($cd->artist);
  $new_cd->insert; # Auto-increment primary key filled in after INSERT
  $new_cd->title('Fork');

  $schema->txn_do(sub { $new_cd->update }); # Runs the update in a transaction

  $millennium_cds_rs->update({ year => 2002 }); # Single-query bulk update

=head1 DESCRIPTION

This is an SQL to OO mapper with an object API inspired by L<Class::DBI>
(and a compatibility layer as a springboard for porting) and a resultset API
that allows abstract encapsulation of database operations. It aims to make
representing queries in your code as perl-ish as possible while still
providing access to as many of the capabilities of the database as possible,
including retrieving related records from multiple tables in a single query,
JOIN, LEFT JOIN, COUNT, DISTINCT, GROUP BY and HAVING support.

DBIx::Class can handle multi-column primary and foreign keys, complex
queries and database-level paging, and does its best to only query the
database in order to return something you've directly asked for. If a
resultset is used as an iterator it only fetches rows off the statement
handle as requested in order to minimise memory usage. It has auto-increment
support for SQLite, MySQL, PostgreSQL, Oracle, SQL Server and DB2 and is
known to be used in production on at least the first four, and is fork-
and thread-safe out of the box (although your DBD may not be).

This project is still under rapid development, so large new features may be
marked EXPERIMENTAL - such APIs are still usable but may have edge bugs.
Failing test cases are *always* welcome and point releases are put out rapidly
as bugs are found and fixed.

We do our best to maintain full backwards compatibility for published
APIs, since DBIx::Class is used in production in many organisations,
and even backwards incompatible changes to non-published APIs will be fixed
if they're reported and doing so doesn't cost the codebase anything.

The test suite is quite substantial, and several developer releases
are generally made to CPAN before the branch for the next release is
merged back to trunk for a major release.

The community can be found via:

  Mailing list: http://lists.scsys.co.uk/mailman/listinfo/dbix-class/

  SVN: http://dev.catalyst.perl.org/repos/bast/DBIx-Class/

  SVNWeb: http://dev.catalyst.perl.org/svnweb/bast/browse/DBIx-Class/

  IRC: irc.perl.org#dbix-class

=head1 WHERE TO GO NEXT

L<DBIx::Class::Manual::DocMap> lists each task you might want help on, and
the modules where you will find documentation.

=head1 AUTHOR

mst: Matt S. Trout <mst@shadowcatsystems.co.uk>

(I mostly consider myself "project founder" these days but the AUTHOR heading
is traditional :)

=head1 CONTRIBUTORS

abraxxa: Alexander Hartmaier <alex_hartmaier@hotmail.com>

aherzog: Adam Herzog <adam@herzogdesigns.com>

andyg: Andy Grundman <andy@hybridized.org>

ank: Andres Kievsky

ash: Ash Berlin <ash@cpan.org>

bert: Norbert Csongradi <bert@cpan.org>

blblack: Brandon L. Black <blblack@gmail.com>

bluefeet: Aran Deltac <bluefeet@cpan.org>

captainL: Luke Saunders <luke.saunders@gmail.com>

castaway: Jess Robinson

claco: Christopher H. Laco

clkao: CL Kao

da5id: David Jack Olrik <djo@cpan.org>

dkubb: Dan Kubb <dan.kubb-cpan@onautopilot.com>

dnm: Justin Wheeler <jwheeler@datademons.com>

draven: Marcus Ramberg <mramberg@cpan.org>

dwc: Daniel Westermann-Clark <danieltwc@cpan.org>

dyfrgi: Michael Leuchtenburg <michael@slashhome.org>

gphat: Cory G Watson <gphat@cpan.org>

jesper: Jesper Krogh

jguenther: Justin Guenther <jguenther@cpan.org>

jnapiorkowski: John Napiorkowski <jjn1056@yahoo.com>

jon: Jon Schutz <jjschutz@cpan.org>

jshirley: J. Shirley <jshirley@gmail.com>

konobi: Scott McWhirter

LTJake: Brian Cassidy <bricas@cpan.org>

mattlaw: Matt Lawrence

ned: Neil de Carteret

nigel: Nigel Metheringham <nigelm@cpan.org>

ningu: David Kamholz <dkamholz@cpan.org>

Numa: Dan Sully <daniel@cpan.org>

oyse: Øystein Torget <oystein.torget@dnv.com>

paulm: Paul Makepeace

penguin: K J Cheetham

perigrin: Chris Prather <chris@prather.org>

phaylon: Robert Sedlacek <phaylon@dunkelheit.at>

quicksilver: Jules Bean

sc_: Just Another Perl Hacker

scotty: Scotty Allen <scotty@scottyallen.com>

semifor: Marc Mims <marc@questright.com>

sszabo: Stephan Szabo <sszabo@bigpanda.com>

Todd Lipcon

Tom Hukins

typester: Daisuke Murase <typester@cpan.org>

victori: Victor Igumnov <victori@cpan.org>

wdh: Will Hawes

willert: Sebastian Willert <willert@cpan.org>

zamolxes: Bogdan Lucaciu <bogdan@wiz.ro>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
