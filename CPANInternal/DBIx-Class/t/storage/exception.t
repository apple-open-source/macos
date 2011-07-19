#!/usr/bin/perl

use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBICTest::Schema;

# make sure nothing eats the exceptions (an unchecked eval in Storage::DESTROY used to be a problem)

{
  package Dying::Storage;

  use warnings;
  use strict;

  use base 'DBIx::Class::Storage::DBI';

  sub _populate_dbh {
    my $self = shift;
    my $death = $self->_dbi_connect_info->[3]{die};

    die "storage test died: $death" if $death eq 'before_populate';
    my $ret = $self->next::method (@_);
    die "storage test died: $death" if $death eq 'after_populate';

    return $ret;
  }
}

for (qw/before_populate after_populate/) {
  dies_ok (sub {
    my $schema = DBICTest::Schema->clone;
    $schema->storage_type ('Dying::Storage');
    $schema->connection (DBICTest->_database, { die => $_ });
    $schema->storage->ensure_connected;
  }, "$_ exception found");
}

done_testing;
