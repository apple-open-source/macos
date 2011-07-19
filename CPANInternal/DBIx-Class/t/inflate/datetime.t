use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval { require DateTime::Format::SQLite };
plan $@
  ? ( skip_all => "Need DateTime::Format::SQLite for DT inflation tests" )
  : ( tests => 18 )
;

# inflation test
my $event = $schema->resultset("Event")->find(1);

isa_ok($event->starts_at, 'DateTime', 'DateTime returned');

# klunky, but makes older Test::More installs happy
my $starts = $event->starts_at;
is("$starts", '2006-04-25T22:24:33', 'Correct date/time');

TODO: {
  local $TODO = "We can't do this yet before 0.09" if DBIx::Class->VERSION < 0.09;

  ok(my $row =
    $schema->resultset('Event')->search({ starts_at => $starts })->single);
  is(eval { $row->id }, 1, 'DT in search');

  ok($row =
    $schema->resultset('Event')->search({ starts_at => { '>=' => $starts } })->single);
  is(eval { $row->id }, 1, 'DT in search with condition');
}

# create using DateTime
my $created = $schema->resultset('Event')->create({
    starts_at => DateTime->new(year=>2006, month=>6, day=>18),
    created_on => DateTime->new(year=>2006, month=>6, day=>23)
});
my $created_start = $created->starts_at;

isa_ok($created->starts_at, 'DateTime', 'DateTime returned');
is("$created_start", '2006-06-18T00:00:00', 'Correct date/time');

## timestamp field
isa_ok($event->created_on, 'DateTime', 'DateTime returned');

## varchar fields
isa_ok($event->varchar_date, 'DateTime', 'DateTime returned');
isa_ok($event->varchar_datetime, 'DateTime', 'DateTime returned');

## skip inflation field
isnt(ref($event->skip_inflation), 'DateTime', 'No DateTime returned for skip inflation column');

# klunky, but makes older Test::More installs happy
my $createo = $event->created_on;
is("$createo", '2006-06-22T21:00:05', 'Correct date/time');

my $created_cron = $created->created_on;

isa_ok($created->created_on, 'DateTime', 'DateTime returned');
is("$created_cron", '2006-06-23T00:00:00', 'Correct date/time');

## varchar field using inflate_date => 1
my $varchar_date = $event->varchar_date;
is("$varchar_date", '2006-07-23T00:00:00', 'Correct date/time');

## varchar field using inflate_datetime => 1
my $varchar_datetime = $event->varchar_datetime;
is("$varchar_datetime", '2006-05-22T19:05:07', 'Correct date/time');

## skip inflation field
my $skip_inflation = $event->skip_inflation;
is ("$skip_inflation", '2006-04-21 18:04:06', 'Correct date/time');
