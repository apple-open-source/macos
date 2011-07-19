use strict;
use Test::More;


BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan skip_all => 'Class::Trigger and DBIx::ContextualFetch required' if $@;
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 3);
}


use lib 't/cdbi/testlib';
use Director;

# Test that has_many() will load the foreign class.
ok !Class::Inspector->loaded( 'Film' );
ok eval { Director->has_many( films => 'Film' ); 1; } || diag $@;

my $shan_hua = Director->create({
    Name    => "Shan Hua",
});

my $inframan = Film->create({
    Title       => "Inframan",
    Director    => "Shan Hua",
});
my $guillotine2 = Film->create({
    Title       => "Flying Guillotine 2",
    Director    => "Shan Hua",
});
my $guillotine = Film->create({
    Title       => "Master of the Flying Guillotine",
    Director    => "Yu Wang",
});

is_deeply [sort $shan_hua->films], [sort $inframan, $guillotine2];