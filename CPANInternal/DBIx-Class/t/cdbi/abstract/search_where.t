use Test::More;

use strict;
use warnings;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@");
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 10);
}

INIT {
  use lib 't/cdbi/testlib';
  use Film;
}


Film->create({ Title => $_, Rating => "PG" }) for ("Superman", "Super Fuzz");
Film->create({ Title => "Batman", Rating => "PG13" });

my $superman = Film->search_where( Title => "Superman" );
is $superman->next->Title, "Superman", "search_where() as iterator";
is $superman->next, undef;

{
    my @supers = Film->search_where({ title => { 'like' => 'Super%' } });
    is_deeply [sort map $_->Title, @supers],
              [sort ("Super Fuzz", "Superman")], 'like';
}
    

my @all = Film->search_where({}, { order_by => "Title ASC" });
is_deeply ["Batman", "Super Fuzz", "Superman"],
          [map $_->Title, @all],
          "order_by ASC";

@all = Film->search_where({}, { order_by => "Title DESC" });
is_deeply ["Superman", "Super Fuzz", "Batman"],
          [map $_->Title, @all],
          "order_by DESC";

@all = Film->search_where({ Rating => "PG" }, { limit => 1, order_by => "Title ASC" });
is_deeply ["Super Fuzz"],
          [map $_->Title, @all],
          "where, limit";

@all = Film->search_where({}, { limit => 2, order_by => "Title ASC" });
is_deeply ["Batman", "Super Fuzz"],
          [map $_->Title, @all],
          "limit";

@all = Film->search_where({}, { offset => 1, order_by => "Title ASC" });
is_deeply ["Super Fuzz", "Superman"],
          [map $_->Title, @all],
          "offset";

@all = Film->search_where({}, { limit => 1, offset => 1, order_by => "Title ASC" });
is_deeply ["Super Fuzz"],
          [map $_->Title, @all],
          "limit + offset";

@all = Film->search_where({}, { limit => 2, offset => 1,
                                limit_dialect => "Top", order_by => "Title ASC"
                              });
is_deeply ["Super Fuzz", "Superman"],
          [map $_->Title, @all],
          "limit_dialect ignored";

