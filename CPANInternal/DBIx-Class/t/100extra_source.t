use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

{
    package DBICTest::ResultSource::OtherSource;
    use strict;
    use warnings;
    use base qw/DBIx::Class::ResultSource::Table/;
}

plan tests => 4;

my $schema = DBICTest->init_schema();
my $artist_source = $schema->source('Artist');

my $new_source = DBICTest::ResultSource::OtherSource->new({
  %$artist_source,
  name           => 'artist_preview',
  _relationships => Storable::dclone( $artist_source->_relationships ),
});

$new_source->add_column('other_col' => { data_type => 'integer', default_value => 1 });

my $warn = '';
local $SIG{__WARN__} = sub { $warn = shift };

{
  $schema->register_extra_source( 'artist->extra' => $new_source );

  my $source = $schema->source('DBICTest::Artist');
  is($source->source_name, 'Artist', 'original source still primary source');
}

{
  my $source = $schema->source('DBICTest::Artist');
  $schema->register_source($source->source_name, $source);
  is($warn, '', "re-registering an existing source under the same name causes no errors");
}

{
  my $new_source_name = 'Artist->preview(artist_preview)';
  $schema->register_source( $new_source_name => $new_source );

  ok(($warn =~ /DBICTest::Artist already has a source, use register_extra_source for additional sources/), 'registering extra source causes errors');
  
  my $source = $schema->source('DBICTest::Artist');
  is($source->source_name, $new_source_name, 'original source still primary source');
}

1;
