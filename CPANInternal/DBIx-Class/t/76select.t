use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

my $rs = $schema->resultset('CD')->search({},
    {
        '+select'   => \ 'COUNT(*)',
        '+as'       => 'count'
    }
);
lives_ok(sub { $rs->first->get_column('count') }, 'additional count rscolumn present');
dies_ok(sub { $rs->first->get_column('nonexistent_column') }, 'nonexistant column requests still throw exceptions');

$rs = $schema->resultset('CD')->search({},
    {
        '+select'   => [ \ 'COUNT(*)', 'title' ],
        '+as'       => [ 'count', 'addedtitle' ]
    }
);
lives_ok(sub { $rs->first->get_column('count') }, 'multiple +select/+as columns, 1st rscolumn present');
lives_ok(sub { $rs->first->get_column('addedtitle') }, 'multiple +select/+as columns, 2nd rscolumn present');

$rs = $schema->resultset('CD')->search({},
    {
        '+select'   => [ \ 'COUNT(*)', 'title' ],
        '+as'       => [ 'count', 'addedtitle' ]
    }
)->search({},
    {
        '+select'   => 'title',
        '+as'       => 'addedtitle2'
    }
);
lives_ok(sub { $rs->first->get_column('count') }, '+select/+as chained search 1st rscolumn present');
lives_ok(sub { $rs->first->get_column('addedtitle') }, '+select/+as chained search 1st rscolumn present');
lives_ok(sub { $rs->first->get_column('addedtitle2') }, '+select/+as chained search 3rd rscolumn present');


# test the from search attribute (gets between the FROM and WHERE keywords, allows arbitrary subselects)
# also shows that outer select attributes are ok (i.e. order_by)
#
# from doesn't seem to be useful without using a scalarref - there were no initial tests >:(
#
my $cds = $schema->resultset ('CD')->search ({}, { order_by => 'me.cdid'}); # make sure order is consistent
cmp_ok ($cds->count, '>', 2, 'Initially populated with more than 2 CDs');

my $table = $cds->result_source->name;
$table = $$table if ref $table eq 'SCALAR';
my $subsel = $cds->search ({}, {
    columns => [qw/cdid title/],
    from => \ "(SELECT cdid, title FROM $table LIMIT 2) me",
});

is ($subsel->count, 2, 'Subselect correctly limited the rs to 2 cds');
is ($subsel->next->title, $cds->next->title, 'First CD title match');
is ($subsel->next->title, $cds->next->title, 'Second CD title match');

is($schema->resultset('CD')->current_source_alias, "me", '$rs->current_source_alias returns "me"');



$rs = $schema->resultset('CD')->search({},
    {
        'join' => 'artist',
        'columns' => ['cdid', 'title', 'artist.name'],
    }
);

is_same_sql_bind (
  $rs->as_query,
  '(SELECT me.cdid, me.title, artist.name FROM cd me  JOIN artist artist ON artist.artistid = me.artist)',
  [],
  'Use of columns attribute results in proper sql'
);

lives_ok(sub {
  $rs->first->get_column('cdid')
}, 'columns 1st rscolumn present');

lives_ok(sub {
  $rs->first->get_column('title')
}, 'columns 2nd rscolumn present');

lives_ok(sub {
  $rs->first->artist->get_column('name')
}, 'columns 3rd rscolumn present');



$rs = $schema->resultset('CD')->search({},
    {
        'join' => 'artist',
        '+columns' => ['cdid', 'title', 'artist.name'],
    }
);

is_same_sql_bind (
  $rs->as_query,
  '(SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track, me.cdid, me.title, artist.name FROM cd me  JOIN artist artist ON artist.artistid = me.artist)',
  [],
  'Use of columns attribute results in proper sql'
);

lives_ok(sub {
  $rs->first->get_column('cdid')
}, 'columns 1st rscolumn present');

lives_ok(sub {
  $rs->first->get_column('title')
}, 'columns 2nd rscolumn present');

lives_ok(sub {
  $rs->first->artist->get_column('name')
}, 'columns 3rd rscolumn present');


$rs = $schema->resultset('CD')->search({'tracks.position' => { -in => [2] } },
  {
    join => 'tracks',
    columns => [qw/me.cdid me.title/],
    '+select' => ['tracks.position'],
    '+as' => ['track_position'],

    # get a hashref of CD1 only (the first with a second track)
    result_class => 'DBIx::Class::ResultClass::HashRefInflator',
    order_by => 'cdid',
    rows => 1,
  }
);

is_deeply (
  $rs->single,
  {
    cdid => 1,
    track_position => 2,
    title => 'Spoonful of bees',
  },
  'limited prefetch via column works on a multi-relationship',
);

my $sub_rs = $rs->search ({},
  {
    columns => [qw/artist tracks.trackid/],    # columns should not be merged but override $rs columns
    '+select' => ['tracks.title'],
    '+as' => ['tracks.title'],
  }
);

is_deeply(
  $sub_rs->single,
  {
    artist         => 1,
    tracks => {
      title => 'Apiary',
      trackid => 17,
    },
  },
  'columns/select/as fold properly on sub-searches',
);

done_testing;
