use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 11;

# Check that you can leave off the alias
{
  my $artist = $schema->resultset('Artist')->find(1);

  my $existing_cd = $artist->search_related('cds', undef, { prefetch => 'tracks' })->find_or_create({
    title => 'Ted',
    year  => 2006,
  });
  ok(! $existing_cd->is_changed, 'find_or_create on prefetched has_many with same column names: row is clean');
  is($existing_cd->title, 'Ted', 'find_or_create on prefetched has_many with same column names: name matches existing entry');

  my $new_cd = $artist->search_related('cds', undef, { prefetch => 'tracks' })->find_or_create({
    title => 'Something Else',
    year  => 2006,
  });
  ok(! $new_cd->is_changed, 'find_or_create on prefetched has_many with same column names: row is clean');
  is($new_cd->title, 'Something Else', 'find_or_create on prefetched has_many with same column names: title matches');
}

# Check that you can specify the alias
{
  my $artist = $schema->resultset('Artist')->find(1);

  my $existing_cd = $artist->search_related('cds', undef, { prefetch => 'tracks' })->find_or_create({
    'me.title' => 'Something Else',
    'me.year'  => 2006,
  });
  ok(! $existing_cd->is_changed, 'find_or_create on prefetched has_many with same column names: row is clean');
  is($existing_cd->title, 'Something Else', 'find_or_create on prefetched has_many with same column names: can be disambiguated with "me." for existing entry');

  my $new_cd = $artist->search_related('cds', undef, { prefetch => 'tracks' })->find_or_create({
    'me.title' => 'Some New Guy',
    'me.year'  => 2006,
  });
  ok(! $new_cd->is_changed, 'find_or_create on prefetched has_many with same column names: row is clean');
  is($new_cd->title, 'Some New Guy', 'find_or_create on prefetched has_many with same column names: can be disambiguated with "me." for new entry');
}

# Don't pass column names with related alias to new_result
{
  my $cd_rs = $schema->resultset('CD')->search({ 'artist.name' => 'Caterwauler McCrae' }, { join => 'artist' });

  my $cd = $cd_rs->find_or_new({ title => 'Huh?', year => 2006 });
  is($cd->in_storage, 0, 'new CD not in storage yet');
  is($cd->title, 'Huh?', 'new CD title is correct');
  is($cd->year, 2006, 'new CD year is correct');
}
