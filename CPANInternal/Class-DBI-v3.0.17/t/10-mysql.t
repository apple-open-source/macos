$| = 1;
use strict;

use Test::More;

eval { require Date::Simple };
plan skip_all => "Need Date::Simple for this test" if $@;

eval { require 't/testlib/MyFoo.pm' };
plan skip_all => "Need MySQL for this test" if $@;

plan tests => 68;

package main;

ok(
	my $bar = MyFoo->insert({ name => "bar", val => 4, tdate => "2000-01-01" }),
	"bar"
);
ok(
	my $baz = MyFoo->insert({ name => "baz", val => 7, tdate => "2000-01-01" }),
	"baz"
);
is($baz->id, $bar->id + 1, "Auto incremented primary key");
is($bar->tdate, Date::Simple->new->format, " .. got today's date");
ok(my $wibble = $bar->copy, "Copy with auto_increment");
is($wibble->id, $baz->id + 1, " .. correct key");
ok(my $wobble = $bar->copy(6), "Copy without auto_increment");
is($wobble->id, 6, " .. correct key");
ok($wobble->tdate('2001-01-01') && $wobble->update, "Set the date of wobble");
isa_ok $wobble->tdate, "Date::Simple";
is($wobble->tdate, Date::Simple->new->format, " but it's set to today");
my $bobble = MyFoo->retrieve($wobble->id);
is($bobble->tdate, Date::Simple->new->format, " set to today in DB too");
isa_ok $bobble->tdate, "Date::Simple";

is MyFoo->count_all, 4, "count_all()";
is MyFoo->minimum_value_of("val"), 4, "min()";
is MyFoo->maximum_value_of("val"), 7, "max()";

require './t/testlib/MyStarLinkMCPK.pm';

ok(my $f1 = MyFilm->insert({ title => "Veronique" }), "Create Veronique");
ok(my $f2 = MyFilm->insert({ title => "Red" }),       "Create Red");

ok(my $s1 = MyStar->insert({ name => "Irene Jacob" }),      "Irene Jacob");
ok(my $s2 = MyStar->insert({ name => "Jerzy Gudejko" }),    "Create Jerzy");
ok(my $s3 = MyStar->insert({ name => "Frédérique Feder" }), "Create Fred");

ok(my $l1 = MyStarLink->insert({ film => $f1, star => $s1 }), "Link 1");
ok(my $l2 = MyStarLink->insert({ film => $f1, star => $s2 }), "Link 2");
ok(my $l3 = MyStarLink->insert({ film => $f2, star => $s1 }), "Link 3");
ok(my $l4 = MyStarLink->insert({ film => $f2, star => $s3 }), "Link 4");

ok(my $lm1 = MyStarLinkMCPK->insert({ film => $f1, star => $s1 }),
	"Link MCPK 1");
ok(my $lm2 = MyStarLinkMCPK->insert({ film => $f1, star => $s2 }),
	"Link MCPK 2");
ok(my $lm3 = MyStarLinkMCPK->insert({ film => $f2, star => $s1 }),
	"Link MCPK 3");
ok(my $lm4 = MyStarLinkMCPK->insert({ film => $f2, star => $s3 }),
	"Link MCPK 4");

{    # Warnings for scalar context?
	my $err = "";
	local $SIG{__WARN__} = sub { $err = $_[0]; };
	$err = "";
	1 if MyStarLinkMCPK->_essential;
	is $err, "", "_essential() tolerates scalar context with multi-column key";

	1 if MyStarLinkMCPK->primary_column;
	like $err, qr/fetching in scalar context/, "but primary_column() complains";
}

# try to create one with duplicate primary key
my $lm5 = eval { MyStarLinkMCPK->insert({ film => $f2, star => $s3 }) };
ok(!$lm5, "Can't insert duplicate");
ok($@ =~ /^Can't insert .* duplicate/i, "Duplicate insert caused exception");

# create one to delete
ok(my $lm6 = MyStarLinkMCPK->insert({ film => $f2, star => $s2 }),
	"Link MCPK 5");
ok(my $lm7 = MyStarLinkMCPK->retrieve(film => $f2, star => $s2),
	"Retrieve from table");
ok($lm7 && $lm7->delete, "Delete from table");
ok(!MyStarLinkMCPK->retrieve(film => $f2, star => $s2), "No longer in table");

# test stringify
is "$lm1", "1/1", "stringify";
is "$lm4", "2/3", "stringify";

my $to_ids = sub { join ":", sort map $_->id, @_ };

{
	my @ver_star = $f1->stars;
	is @ver_star, 2, "Veronique has 2 stars ";
	isa_ok $ver_star[0] => 'MyStar';
	is $to_ids->(@ver_star), $to_ids->($s1, $s2), "Correct stars";
}

{
	my @irene = $s1->films;
	is @irene, 2, "Irene Jacob has 2 films";
	isa_ok $irene[0] => 'MyFilm';
	is $to_ids->(@irene), $to_ids->($f1, $f2), "Correct films";
}

{
	my @jerzy = $s2->films;
	is @jerzy, 1, "Jerzy has 1 film";
	is $jerzy[0]->id, $f1->id, " Veronique";
}

{
	ok MyStar->has_many(filmids => [ MyStarLink => 'film', 'id' ]),
		"**** Multi-map";
	my @filmid = $s1->filmids;
	ok !ref $filmid[0], "Film-id is not a reference";

	my $first = $s1->filmids->first;
	ok !ref $first, "First is not a reference";
	is $first, $filmid[0], "But it's the same as filmid[0]";
}

{    # cascades correctly
	my $lenin  = MyFilm->insert({ title    => "Leningrad Cowboys Go America" });
	my $pimme  = MyStar->insert({ name     => "Pimme Korhonen" });
	my $cowboy = MyStarLink->insert({ film => $lenin, star => $pimme });
	$lenin->delete;
	is MyStar->search(name => 'Pimme Korhonen')->count, 1, "Pimme still exists";
	is MyStarLink->search(star => $pimme->id)->count, 0, "But in no films";
}

{
	ok MyStar->has_many(filmids_mcpk => [ MyStarLinkMCPK => 'film', 'id' ]),
		"**** Multi-map via MCPK";
	my @filmid = $s1->filmids_mcpk;
	ok !ref $filmid[0], "Film-id is not a reference";

	my $first = $s1->filmids_mcpk->first;
	ok !ref $first, "First is not a reference";
	is $first, $filmid[0], "But it's the same as filmid[0]";
}

{
	ok my $f0 = MyFilm->insert({ filmid => 0, title => "Year 0" }),
		"Create with explicit id = 0";
	isa_ok $f0 => 'MyFilm';
	is $f0->id, 0, "ID of 0";
}

{    # create doesn't mess with my hash.
	my %info = (Name => "Catherine Wilkening");
	my $cw = MyStar->find_or_create(\%info);
	is scalar keys %info, 1, "Our hash still has only one key";
	is $info{Name}, "Catherine Wilkening", "Still same name";
}

{
	MyFilm->set_sql(
		retrieve_all_sorted => "SELECT __ESSENTIAL__ FROM __TABLE__ ORDER BY %s");

	sub MyFilm::retrieve_all_sorted_by {
		my ($class, $order_by) = @_;
		return $class->sth_to_objects($class->sql_retrieve_all_sorted($order_by));
	}

	my @all = MyFilm->retrieve_all_sorted_by("title");
	is @all, 3, "3 films";
	ok $all[2]->title gt $all[1]->title && $all[1]->title gt $all[0]->title,
		"sorted by title";
}

{

	package Class::DBI::Search::Test::Limited;
	use base 'Class::DBI::Search::Basic';

	sub fragment {
		my $self = shift;
		my $frag = $self->SUPER::fragment;
		if (defined(my $limit = $self->opt('limit'))) {
			$frag .= " LIMIT $limit";
		}
		return $frag;
	}

	package main;

	MyFilm->add_searcher(search => "Class::DBI::Search::Test::Limited");

	my @common = map MyFilm->insert({ title => "Common Title" }), 1 .. 3;
	{
		my @ltd = MyFilm->search(
			title => "Common Title",
			{
				order_by => 'filmid',
				limit    => 1
			}
		);
		is @ltd, 1, "Limit to one film";
		is $ltd[0]->id, $common[0]->id, "The correct one";
	}

	{
		my @ltd = MyFilm->search(
			title => "Common Title",
			{
				order_by => 'filmid',
				limit    => "1,1"
			}
		);
		is @ltd, 1, "Limit to middle film";
		is $ltd[0]->id, $common[1]->id, " - correctly";
	}

}
