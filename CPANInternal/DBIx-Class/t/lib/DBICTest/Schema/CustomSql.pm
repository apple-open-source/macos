package # hide from PAUSE 
    DBICTest::Schema::CustomSql;

use base qw/DBICTest::Schema::Artist/;

__PACKAGE__->table('dummy');

__PACKAGE__->result_source_instance->name(\<<SQL);
  ( SELECT a.*, cd.cdid AS cdid, cd.title AS title, cd.year AS year 
  FROM artist a
  JOIN cd ON cd.artist = a.artistid
  WHERE cd.year = ?)
SQL

sub sqlt_deploy_hook { $_[1]->schema->drop_table($_[1]) }

1;
