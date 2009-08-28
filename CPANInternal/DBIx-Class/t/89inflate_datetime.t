use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval { require DateTime::Format::MySQL };
plan skip_all => "Need DateTime::Format::MySQL for inflation tests" if $@;

plan tests => 17;

# inflation test
my $event = $schema->resultset("Event")->find(1);

isa_ok($event->starts_at, 'DateTime', 'DateTime returned');

# klunky, but makes older Test::More installs happy
my $starts = $event->starts_at;
is("$starts", '2006-04-25T22:24:33', 'Correct date/time');

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

# klunky, but makes older Test::More installs happy
my $createo = $event->created_on;
is("$createo", '2006-06-22T21:00:05', 'Correct date/time');

my $created_cron = $created->created_on;

isa_ok($created->created_on, 'DateTime', 'DateTime returned');
is("$created_cron", '2006-06-23T00:00:00', 'Correct date/time');


# Test "timezone" parameter
my $event_tz = $schema->resultset('EventTZ')->create({
    starts_at => DateTime->new(year=>2007, month=>12, day=>31, time_zone => "America/Chicago" ),
    created_on => DateTime->new(year=>2006, month=>1, day=>31,
        hour => 13, minute => 34, second => 56, time_zone => "America/New_York" ),
});

my $starts_at = $event_tz->starts_at;
is("$starts_at", '2007-12-31T00:00:00', 'Correct date/time using timezone');

my $created_on = $event_tz->created_on;
is("$created_on", '2006-01-31T12:34:56', 'Correct timestamp using timezone');
is($event_tz->created_on->time_zone->name, "America/Chicago", "Correct timezone");

my $loaded_event = $schema->resultset('EventTZ')->find( $event_tz->id );

isa_ok($loaded_event->starts_at, 'DateTime', 'DateTime returned');
$starts_at = $loaded_event->starts_at;
is("$starts_at", '2007-12-31T00:00:00', 'Loaded correct date/time using timezone');
is($starts_at->time_zone->name, 'America/Chicago', 'Correct timezone');

isa_ok($loaded_event->created_on, 'DateTime', 'DateTime returned');
$created_on = $loaded_event->created_on;
is("$created_on", '2006-01-31T12:34:56', 'Loaded correct timestamp using timezone');
is($created_on->time_zone->name, 'America/Chicago', 'Correct timezone');

