use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 91;

my $schema = DBICTest->init_schema();

lives_ok ( sub {
  my $cd = $schema->resultset('CD')->create({
    artist => { 
      name => 'Fred Bloggs' 
    },
    title => 'Some CD',
    year => 1996
  });

  isa_ok($cd, 'DBICTest::CD', 'Created CD object');
  isa_ok($cd->artist, 'DBICTest::Artist', 'Created related Artist');
  is($cd->artist->name, 'Fred Bloggs', 'Artist created correctly');
}, 'simple create + parent (the stuff $rs belongs_to) ok');

lives_ok ( sub {
  my $bm_rs = $schema->resultset('Bookmark');
  my $bookmark = $bm_rs->create({
    link => {
      id => 66,
    },
  });

  isa_ok($bookmark, 'DBICTest::Bookmark', 'Created Bookrmark object');
  isa_ok($bookmark->link, 'DBICTest::Link', 'Created related Link');
  is (
    $bm_rs->search (
      { 'link.title' => $bookmark->link->title },
      { join => 'link' },
    )->count,
    1,
    'Bookmark and link made it to the DB',
  );
}, 'simple create where the child and parent have no values, except for an explicit parent pk ok');

lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  my $cd = $artist->create_related (cds => {
    title => 'Music to code by',
    year => 2007,
    tags => [
      { 'tag' => 'rock' },
    ],
  });

  isa_ok($cd, 'DBICTest::CD', 'Created CD');
  is($cd->title, 'Music to code by', 'CD created correctly');
  is($cd->tags->count, 1, 'One tag created for CD');
  is($cd->tags->first->tag, 'rock', 'Tag created correctly');

}, 'create over > 1 levels of has_many create (A => { has_many => { B => has_many => C } } )');

throws_ok (
  sub {
    # Create via update - add a new CD <--- THIS SHOULD HAVE NEVER WORKED!
    $schema->resultset('Artist')->first->update({
      cds => [
        { title => 'Yet another CD',
          year => 2006,
        },
      ],
    });
  },
  qr/Recursive update is not supported over relationships of type 'multi'/,
  'create via update of multi relationships throws an exception'
);

lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  my $c2p = $schema->resultset('CD_to_Producer')->create ({
    cd => {
      artist => $artist,
      title => 'Bad investment',
      year => 2008,
      tracks => [
        { title => 'Just buy' },
        { title => 'Why did we do it' },
        { title => 'Burn baby burn' },
      ],
    },
    producer => {
      name => 'Lehman Bros.',
    },
  });

  isa_ok ($c2p, 'DBICTest::CD_to_Producer', 'Linker object created');
  my $prod = $schema->resultset ('Producer')->find ({ name => 'Lehman Bros.' });
  isa_ok ($prod, 'DBICTest::Producer', 'Producer row found');
  is ($prod->cds->count, 1, 'Producer has one production');
  my $cd = $prod->cds->first;
  is ($cd->title, 'Bad investment', 'CD created correctly');
  is ($cd->tracks->count, 3, 'CD has 3 tracks');
}, 'Create m2m while originating in the linker table');


#CD -> has_many -> Tracks -> might have -> Single -> has_many -> Tracks
#                                               \
#                                                \-> has_many \
#                                                              --> CD2Producer
#                                                /-> has_many /
#                                               /
#                                          Producer
lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  my $cd = $schema->resultset('CD')->create ({
    artist => $artist,
    title => 'Music to code by at night',
    year => 2008,
    tracks => [
      {
        title => 'Off by one again',
      },
      {
        title => 'The dereferencer',
        cd_single => {
          artist => $artist,
          year => 2008,
          title => 'Was that a null (Single)',
          tracks => [
            { title => 'The dereferencer' },
            { title => 'The dereferencer II' },
          ],
          cd_to_producer => [
            {
              producer => {
                name => 'K&R',
              }
            },
            {
              producer => {
                name => 'Don Knuth',
              }
            },
          ]
        },
      },
    ],
  });

  isa_ok ($cd, 'DBICTest::CD', 'Main CD object created');
  is ($cd->title, 'Music to code by at night', 'Correct CD title');
  is ($cd->tracks->count, 2, 'Two tracks on main CD');

  my ($t1, $t2) = $cd->tracks->all;
  is ($t1->title, 'Off by one again', 'Correct 1st track name');
  is ($t1->cd_single, undef, 'No single for 1st track');
  is ($t2->title, 'The dereferencer', 'Correct 2nd track name');
  isa_ok ($t2->cd_single, 'DBICTest::CD', 'Created a single for 2nd track');

  my $single = $t2->cd_single;
  is ($single->tracks->count, 2, 'Two tracks on single CD');
  is ($single->tracks->find ({ position => 1})->title, 'The dereferencer', 'Correct 1st track title');
  is ($single->tracks->find ({ position => 2})->title, 'The dereferencer II', 'Correct 2nd track title');

  is ($single->cd_to_producer->count, 2, 'Two producers created for the single cd');
  is_deeply (
    [ sort map { $_->producer->name } ($single->cd_to_producer->all) ],
    ['Don Knuth', 'K&R'],
    'Producers named correctly',
  );
}, 'Create over > 1 levels of might_have with multiple has_many and multiple m2m but starting at a has_many level');

#Track -> might have -> Single -> has_many -> Tracks
#                           \
#                            \-> has_many \
#                                          --> CD2Producer
#                            /-> has_many /
#                           /
#                       Producer
lives_ok ( sub {
  my $cd = $schema->resultset('CD')->first;
  my $track = $schema->resultset('Track')->create ({
    cd => $cd,
    title => 'Multicreate rocks',
    cd_single => {
      artist => $cd->artist,
      year => 2008,
      title => 'Disemboweling MultiCreate',
      tracks => [
        { title => 'Why does mst write this way' },
        { title => 'Chainsaw celebration' },
        { title => 'Purl cleans up' },
      ],
      cd_to_producer => [
        {
          producer => {
            name => 'mst',
          }
        },
        {
          producer => {
            name => 'castaway',
          }
        },
        {
          producer => {
            name => 'theorbtwo',
          }
        },
      ]
    },
  });

  isa_ok ($track, 'DBICTest::Track', 'Main Track object created');
  is ($track->title, 'Multicreate rocks', 'Correct Track title');

  my $single = $track->cd_single;
  isa_ok ($single, 'DBICTest::CD', 'Created a single with the track');
  is ($single->tracks->count, 3, '3 tracks on single CD');
  is ($single->tracks->find ({ position => 1})->title, 'Why does mst write this way', 'Correct 1st track title');
  is ($single->tracks->find ({ position => 2})->title, 'Chainsaw celebration', 'Correct 2nd track title');
  is ($single->tracks->find ({ position => 3})->title, 'Purl cleans up', 'Correct 3rd track title');

  is ($single->cd_to_producer->count, 3, '3 producers created for the single cd');
  is_deeply (
    [ sort map { $_->producer->name } ($single->cd_to_producer->all) ],
    ['castaway', 'mst', 'theorbtwo'],
    'Producers named correctly',
  );
}, 'Create over > 1 levels of might_have with multiple has_many and multiple m2m but starting at the might_have directly');

lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  my $cd = $schema->resultset('CD')->create ({
    artist => $artist,
    title => 'Music to code by at twilight',
    year => 2008,
    artwork => {
      images => [
        { name => 'recursive descent' },
        { name => 'tail packing' },
      ],
    },
  });

  isa_ok ($cd, 'DBICTest::CD', 'Main CD object created');
  is ($cd->title, 'Music to code by at twilight', 'Correct CD title');
  isa_ok ($cd->artwork, 'DBICTest::Artwork', 'Artwork created');

  # this test might look weird, but it failed at one point, keep it there
  my $art_obj = $cd->artwork;
  ok ($art_obj->has_column_loaded ('cd_id'), 'PK/FK present on artwork object');
  is ($art_obj->images->count, 2, 'Correct artwork image count via the new object');
  is_deeply (
    [ sort $art_obj->images->get_column ('name')->all ],
    [ 'recursive descent', 'tail packing' ],
    'Images named correctly in objects',
  );

  my $artwork = $schema->resultset('Artwork')->search (
    { 'cd.title' => 'Music to code by at twilight' },
    { join => 'cd' },
  )->single;

  is ($artwork->images->count, 2, 'Correct artwork image count via a new search');

  is_deeply (
    [ sort $artwork->images->get_column ('name')->all ],
    [ 'recursive descent', 'tail packing' ],
    'Images named correctly after search',
  );
}, 'Test might_have again but with a PK == FK in the middle (obviously not specified)');

lives_ok ( sub {
  my $cd = $schema->resultset('CD')->first;
  my $track = $schema->resultset ('Track')->create ({
    cd => $cd,
    title => 'Black',
    lyrics => {
      lyric_versions => [
        { text => 'The color black' },
        { text => 'The colour black' },
      ],
    },
  });

  isa_ok ($track, 'DBICTest::Track', 'Main track object created');
  is ($track->title, 'Black', 'Correct track title');
  isa_ok ($track->lyrics, 'DBICTest::Lyrics', 'Lyrics created');

  # this test might look weird, but it was failing at one point, keep it there
  my $lyric_obj = $track->lyrics;
  ok ($lyric_obj->has_column_loaded ('lyric_id'), 'PK present on lyric object');
  ok ($lyric_obj->has_column_loaded ('track_id'), 'FK present on lyric object');
  is ($lyric_obj->lyric_versions->count, 2, 'Correct lyric versions count via the new object');
  is_deeply (
    [ sort $lyric_obj->lyric_versions->get_column ('text')->all ],
    [ 'The color black', 'The colour black' ],
    'Lyrics text in objects matches',
  );


  my $lyric = $schema->resultset('Lyrics')->search (
    { 'track.title' => 'Black' },
    { join => 'track' },
  )->single;

  is ($lyric->lyric_versions->count, 2, 'Correct lyric versions count via a new search');

  is_deeply (
    [ sort $lyric->lyric_versions->get_column ('text')->all ],
    [ 'The color black', 'The colour black' ],
    'Lyrics text via search matches',
  );
}, 'Test might_have again but with just a PK and FK (neither specified) in the mid-table');

lives_ok ( sub {
  my $newartist2 = $schema->resultset('Artist')->find_or_create({ 
    name => 'Fred 3',
    cds => [
      { 
        title => 'Noah Act',
        year => 2007,
      },
    ],
  });
  is($newartist2->name, 'Fred 3', 'Created new artist with cds via find_or_create');
}, 'Nested find_or_create');

lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  
  my $cd_result = $artist->create_related('cds', {
  
    title => 'TestOneCD1',
    year => 2007,
    tracks => [
      { title => 'TrackOne' },
      { title => 'TrackTwo' },
    ],

  });
  
  isa_ok( $cd_result, 'DBICTest::CD', "Got Good CD Class");
  ok( $cd_result->title eq "TestOneCD1", "Got Expected Title");
  
  my $tracks = $cd_result->tracks;
  
  isa_ok( $tracks, 'DBIx::Class::ResultSet', 'Got Expected Tracks ResultSet');
  
  foreach my $track ($tracks->all)
  {
    isa_ok( $track, 'DBICTest::Track', 'Got Expected Track Class');
  }
}, 'First create_related pass');

lives_ok ( sub {
  my $artist = $schema->resultset('Artist')->first;
  
  my $cd_result = $artist->create_related('cds', {
  
    title => 'TestOneCD2',
    year => 2007,
    tracks => [
      { title => 'TrackOne' },
      { title => 'TrackTwo' },
    ],

    liner_notes => { notes => 'I can haz liner notes?' },

  });
  
  isa_ok( $cd_result, 'DBICTest::CD', "Got Good CD Class");
  ok( $cd_result->title eq "TestOneCD2", "Got Expected Title");
  ok( $cd_result->notes eq 'I can haz liner notes?', 'Liner notes');
  
  my $tracks = $cd_result->tracks;
  
  isa_ok( $tracks, 'DBIx::Class::ResultSet', "Got Expected Tracks ResultSet");
  
  foreach my $track ($tracks->all)
  {
    isa_ok( $track, 'DBICTest::Track', 'Got Expected Track Class');
  }
}, 'second create_related with same arguments');

lives_ok ( sub {
  my $cdp = $schema->resultset('CD_to_Producer')->create({
    cd => { artist => 1, title => 'foo', year => 2000 },
    producer => { name => 'jorge' }
  });
  ok($cdp, 'join table record created ok');
}, 'create of parents of a record linker table');

lives_ok ( sub {
  my $kurt_cobain = { name => 'Kurt Cobain' };

  my $in_utero = $schema->resultset('CD')->new({
      title => 'In Utero',
      year  => 1993
    });

  $kurt_cobain->{cds} = [ $in_utero ];


  $schema->resultset('Artist')->populate([ $kurt_cobain ]); # %)
  $a = $schema->resultset('Artist')->find({name => 'Kurt Cobain'});

  is($a->name, 'Kurt Cobain', 'Artist insertion ok');
  is($a->cds && $a->cds->first && $a->cds->first->title, 
      'In Utero', 'CD insertion ok');
}, 'populate');

## Create foreign key col obj including PK
## See test 20 in 66relationships.t
lives_ok ( sub {
  my $new_cd_hashref = { 
    cdid => 27, 
    title => 'Boogie Woogie', 
    year => '2007', 
    artist => { artistid => 17, name => 'king luke' }
  };

  my $cd = $schema->resultset("CD")->find(1);

  is($cd->artist->id, 1, 'rel okay');

  my $new_cd = $schema->resultset("CD")->create($new_cd_hashref);
  is($new_cd->artist->id, 17, 'new id retained okay');
}, 'Create foreign key col obj including PK');

lives_ok ( sub {
  $schema->resultset("CD")->create({ 
              cdid => 28, 
              title => 'Boogie Wiggle', 
              year => '2007', 
              artist => { artistid => 18, name => 'larry' }
             });
}, 'new cd created without clash on related artist');

throws_ok ( sub {
    my $t = $schema->resultset("Track")->new({ cd => { artist => undef } });
    #$t->cd($t->new_related('cd', { artist => undef } ) );
    #$t->{_rel_in_storage} = 0;
    $t->insert;
}, qr/cd.artist may not be NULL/, "Exception propogated properly");

lives_ok ( sub {
  $schema->resultset('CD')->create ({
    artist => {
      name => 'larry', # should already exist
    },
    title => 'Warble Marble',
    year => '2009',
    cd_to_producer => [
      { producer => { name => 'Cowboy Neal' } },
    ],
  });

  my $m2m_cd = $schema->resultset('CD')->search ({ title => 'Warble Marble'});
  is ($m2m_cd->count, 1, 'One CD row created via M2M create');
  is ($m2m_cd->first->producers->count, 1, 'CD row created with one producer');
  is ($m2m_cd->first->producers->first->name, 'Cowboy Neal', 'Correct producer row created');
}, 'Test multi create over many_to_many');

1;
