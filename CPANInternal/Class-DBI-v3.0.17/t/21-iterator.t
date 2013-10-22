use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 33);
}

use lib 't/testlib';
use Film;

Film->retrieve_all->delete_all;

my @film  = (
	Film->insert({ Title => 'Film 1' }),
	Film->insert({ Title => 'Film 2' }),
	Film->insert({ Title => 'Film 3' }),
	Film->insert({ Title => 'Film 4' }),
	Film->insert({ Title => 'Film 5' }),
	Film->insert({ Title => 'Film 6' }),
);

{
	my $it1 = Film->retrieve_all;
	isa_ok $it1, "Class::DBI::Iterator";

	my $it2 = Film->retrieve_all;
	isa_ok $it2, "Class::DBI::Iterator";

	while (my $from1 = $it1->next) {
		my $from2 = $it2->next;
		is $from1->id, $from2->id, "Both iterators get $from1";
	}
}

{
	my $it = Film->retrieve_all;
	is $it->first->title, "Film 1", "Film 1 first";
	is $it->next->title, "Film 2", "Film 2 next";
	is $it->first->title, "Film 1", "First goes back to 1";
	is $it->next->title, "Film 2", "With 2 still next";
	$it->reset;
	is $it->next->title, "Film 1", "Reset brings us to film 1 again";
	is $it->next->title, "Film 2", "And 2 is still next";
}


{
	my $it = Film->retrieve_all;
	my @slice = $it->slice(2,4);
	is @slice, 3, "correct slice size (array)";
	is $slice[0]->title, "Film 3", "Film 3 first";
	is $slice[2]->title, "Film 5", "Film 5 last";
}

{
	my $it = Film->retrieve_all;
	my $slice = $it->slice(2,4);
	isa_ok $slice, "Class::DBI::Iterator", "slice as iterator";
	is $slice->count, 3,"correct slice size (array)";
	is $slice->first->title, "Film 3", "Film 3 first";
	is $slice->next->title, "Film 4", "Film 4 next";
	is $slice->first->title, "Film 3", "First goes back to 3";
	is $slice->next->title, "Film 4", "With 4 still next";
	$slice->reset;
	is $slice->next->title, "Film 3", "Reset brings us to film 3 again";
	is $slice->next->title, "Film 4", "And 4 is still next";

	# check if the original iterator still works
	is $it->count, 6, "back to the original iterator, is of right size";
	is $it->first->title, "Film 1", "Film 1 first";
	is $it->next->title, "Film 2", "Film 2 next";
	is $it->first->title, "Film 1", "First goes back to 1";
	is $it->next->title, "Film 2", "With 2 still next";
	is $it->next->title, "Film 3", "Film 3 is still in original Iterator";
	$it->reset;
	is $it->next->title, "Film 1", "Reset brings us to film 1 again";
	is $it->next->title, "Film 2", "And 2 is still next";
}
