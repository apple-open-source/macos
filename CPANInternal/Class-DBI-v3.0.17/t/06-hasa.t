use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 24);
}

@YA::Film::ISA = 'Film';

local $SIG{__WARN__} = sub { };

INIT {
	use lib 't/testlib';
	use Film;
	use Director;
}

Film->create_test_film;
ok(my $btaste = Film->retrieve('Bad Taste'), "We have Bad Taste");
ok(my $pj = $btaste->Director, "Bad taste hasa() director");
ok(!ref($pj), ' ... which is not an object');

ok(Film->has_a('Director' => 'Director'), "Link Director table");
ok(
	Director->insert(
		{
			Name     => 'Peter Jackson',
			Birthday => -300000000,
			IsInsane => 1
		}
	),
	'insert Director'
);

$btaste = Film->retrieve('Bad Taste');

ok($pj = $btaste->Director, "Bad taste now hasa() director");
isa_ok($pj => 'Director');
is($pj->id, 'Peter Jackson', ' ... and is the correct director');

# Oh no!  Its Peter Jacksons even twin, Skippy!  Born one minute after him.
my $sj = Director->insert(
	{
		Name     => 'Skippy Jackson',
		Birthday => (-300000000 + 60),
		IsInsane => 1,
	}
);

is($sj->id, 'Skippy Jackson', 'We have a new director');

Film->has_a(CoDirector => 'Director');

$btaste->CoDirector($sj);
$btaste->update;
is($btaste->CoDirector->Name, 'Skippy Jackson', 'He co-directed');
is(
	$btaste->Director->Name,
	'Peter Jackson',
	"Didnt interfere with each other"
);

{    # Ensure search can take an object
	my @films = Film->search(Director => $pj);
	is @films, 1, "1 Film directed by $pj";
	is $films[0]->id, "Bad Taste", "Bad Taste";
}

inheriting_hasa();

{

	# Skippy directs a film and Peter helps!
	$sj = Director->retrieve('Skippy Jackson');
	$pj = Director->retrieve('Peter Jackson');

	fail_with_bad_object($sj, $btaste);
	taste_bad($sj, $pj);
}

sub inheriting_hasa {
	my $btaste = YA::Film->retrieve('Bad Taste');
	is(ref($btaste->Director),    'Director',       'inheriting hasa()');
	is(ref($btaste->CoDirector),  'Director',       'inheriting hasa()');
	is($btaste->CoDirector->Name, 'Skippy Jackson', ' ... correctly');
}

sub taste_bad {
	my ($dir, $codir) = @_;
	my $tastes_bad = YA::Film->insert(
		{
			Title             => 'Tastes Bad',
			Director          => $dir,
			CoDirector        => $codir,
			Rating            => 'R',
			NumExplodingSheep => 23
		}
	);
	is($tastes_bad->_Director_accessor, 'Skippy Jackson', 'Director_accessor');
	is($tastes_bad->Director->Name,     'Skippy Jackson', 'Director');
	is($tastes_bad->CoDirector->Name,   'Peter Jackson',  'CoDirector');
	is(
		$tastes_bad->_CoDirector_accessor,
		'Peter Jackson',
		'CoDirector_accessor'
	);
}

sub fail_with_bad_object {
	my ($dir, $codir) = @_;
	eval {
		YA::Film->insert(
			{
				Title             => 'Tastes Bad',
				Director          => $dir,
				CoDirector        => $codir,
				Rating            => 'R',
				NumExplodingSheep => 23
			}
		);
	};
	ok $@, $@;
}

package Foo;
use base 'CDBase';
__PACKAGE__->table('foo');
__PACKAGE__->columns('All' => qw/ id fav /);

# fav is a film
__PACKAGE__->db_Main->do(
	qq{
     CREATE TABLE foo (
	     id        INTEGER,
	     fav       VARCHAR(255)
     )
}
);

package Bar;
use base 'CDBase';
__PACKAGE__->table('bar');
__PACKAGE__->columns('All' => qw/ id fav /);

# fav is a foo
__PACKAGE__->db_Main->do(
	qq{
     CREATE TABLE bar (
	     id        INTEGER,
	     fav       INTEGER
     )
}
);

package main;
Foo->has_a("fav" => "Film");
Bar->has_a("fav" => "Foo");
my $foo = Foo->insert({ id => 6, fav => 'Bad Taste' });
my $bar = Bar->insert({ id => 2, fav => 6 });
isa_ok($bar->fav, "Foo");
isa_ok($foo->fav, "Film");

{
	my $foo;
	Foo->add_trigger(after_create => sub { $foo = shift->fav });
	my $gwh = Foo->insert({ id => 93, fav => 'Good Will Hunting' });
	isa_ok $foo, "Film", "Object in after_create trigger";
}

__END__
# TODO: breaks t/10
# http://lists.digitalcraftsmen.net/pipermail/classdbi/2005-November/000610.html 
# test has_a() on primary keys
package MultiKey;
use base 'CDBase';
__PACKAGE__->table('multikey');
__PACKAGE__->columns('Primary' => qw/ id film /);
__PACKAGE__->has_a(film => "Film");
__PACKAGE__->db_Main->do(
	qq{
     CREATE TABLE multikey (
	     id        INTEGER,
     film      VARCHAR(255)
     )
}
);

package main;
my $from_scalar = MultiKey->create({ id => 7, film => 'Bad Taste' });
isa_ok($from_scalar->film(), "Film");

