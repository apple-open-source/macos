package #hide from pause
  DBICTest::BaseResultSet;

use strict;
use warnings;

use base qw/DBIx::Class::ResultSet/;

sub hri_dump {
  return shift->search ({}, { result_class => 'DBIx::Class::ResultClass::HashRefInflator' });
}

1;
