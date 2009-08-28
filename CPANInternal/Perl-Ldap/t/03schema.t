#!perl -w
use Net::LDAP::Schema;

print "1..7\n";

my $schema = Net::LDAP::Schema->new( "data/schema.in" ) or die "Cannot open schema";
print "ok 1\n";

my @atts = $schema->all_attributes();
print "not " unless @atts == 55;
print "ok 2\n";

print "The schema contains ", scalar @atts, " attributes\n";

my @ocs = $schema->all_objectclasses();
print "not " unless @ocs == 22;
print "ok 3\n";
print "The schema contains ", scalar @ocs, " object classes\n";

@atts = $schema->must( "person" );
print "not " unless join(' ', sort map $_->{name}, @atts) eq join(' ',sort qw(cn sn objectClass));
print "ok 4\n";
print "The 'person' OC must have these attributes [",
		join( ",", map $_->{name}, @atts ),
		"]\n";
@atts = $schema->may( "mhsOrganizationalUser" );
print "not " if @atts;
print "ok 5\n";
print "The 'mhsOrganizationalUser' OC may have these attributes [",
		join( ",", map $_->{name}, @atts ),
		"]\n";

print "not " if defined $schema->attribute('distinguishedName')->{max_length};
print "ok 6\n";

print "not " unless $schema->attribute('userPassword')->{max_length} == 128;
print "ok 7\n";

use Data::Dumper;
print Dumper($schema);
