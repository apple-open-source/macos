use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use Test::More;

plan tests => 15;

my $schema = DBICTest->init_schema();
my $rs = $schema->resultset( 'CD' );

{
  my $a = 'artist';
  my $b = 'cd';
  my $expected = [ 'artist', 'cd' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist' ];
  my $b = [ 'cd' ];
  my $expected = [ 'artist', 'cd' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd' ];
  my $b = [ 'cd' ];
  my $expected = [ 'artist', 'cd' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'artist' ];
  my $b = [ 'artist', 'cd' ];
  my $expected = [ 'artist', 'artist', 'cd' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd' ];
  my $b = [ 'artist', 'artist' ];
  my $expected = [ 'artist', 'cd', 'artist' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'twokeys' ];
  my $b = [ 'cds', 'cds' ];
  my $expected = [ 'twokeys', 'cds', 'cds' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = 'artist';
  my $expected = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = [ 'artist', 'cd' ];
  my $expected = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = { 'artist' => 'manager' };
  my $expected = [ 'artist', 'cd', { 'artist' => [ 'manager' ] } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = { 'artist' => 'agent' };
  my $expected = [ { 'artist' => 'agent' }, 'cd', { 'artist' => 'manager' } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = { 'artist' => { 'manager' => 'artist' } };
  my $expected = [ 'artist', 'cd', { 'artist' => [ { 'manager' => 'artist' } ] } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = { 'artist' => { 'manager' => [ 'artist', 'label' ] } };
  my $expected = [ 'artist', 'cd', { 'artist' => [ { 'manager' => [ 'artist', 'label' ] } ] } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd', { 'artist' => 'manager' } ];
  my $b = { 'artist' => { 'tour_manager' => [ 'venue', 'roadie' ] } };
  my $expected = [ { 'artist' => { 'tour_manager' => [ 'venue', 'roadie' ] } }, 'cd', { 'artist' =>  'manager' } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ 'artist', 'cd' ];
  my $b = { 'artist' => { 'tour_manager' => [ 'venue', 'roadie' ] } };
  my $expected = [ { 'artist' => { 'tour_manager' => [ 'venue', 'roadie' ] } }, 'cd' ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}

{
  my $a = [ { 'artist' => 'manager' }, 'cd' ];
  my $b = [ 'artist', { 'artist' => 'manager' } ];
  my $expected = [ { 'artist' => 'manager' }, 'cd', { 'artist' => 'manager' } ];
  my $result = $rs->_merge_attr($a, $b);
  is_deeply( $result, $expected );
}


1;
