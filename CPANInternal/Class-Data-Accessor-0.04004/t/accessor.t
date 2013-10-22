use strict;
use warnings;
use Test::More tests => 19;

package Ray;
use base qw(Class::Data::Accessor);
Ray->mk_classaccessors('Ubu');
Ray->mk_classaccessor(DataFile => '/etc/stuff/data');

package Gun;
use base qw(Ray);
Gun->Ubu('Pere');

package Suitcase;
use base qw(Gun);
Suitcase->DataFile('/etc/otherstuff/data');

package main;

foreach my $class (qw/Ray Gun Suitcase/) {
	can_ok $class =>
		qw/mk_classaccessor Ubu _Ubu_accessor DataFile _DataFile_accessor/;
}

# Test that superclasses effect children.
is +Gun->Ubu, 'Pere', 'Ubu in Gun';
is +Suitcase->Ubu, 'Pere', "Inherited into children";
is +Ray->Ubu, undef, "But not set in parent";

# Set value with data
is +Ray->DataFile, '/etc/stuff/data', "Ray datafile";
is +Gun->DataFile, '/etc/stuff/data', "Inherited into gun";
is +Suitcase->DataFile, '/etc/otherstuff/data', "Different in suitcase";

# Now set the parent
ok +Ray->DataFile('/tmp/stuff'), "Set data in parent";
is +Ray->DataFile, '/tmp/stuff', " - it sticks";
is +Gun->DataFile, '/tmp/stuff', "filters down to unchanged children";
is +Suitcase->DataFile, '/etc/otherstuff/data', "but not to changed";


my $obj = bless {}, 'Gun';
eval { $obj->mk_classaccessor('Ubu') };
ok $@ =~ /^mk_classaccessor\(\) is a class method, not an object method/,
"Can't create classaccessor for an object";

is $obj->DataFile, "/tmp/stuff", "But objects can access the data";

is $obj->DataFile("/tmp/morestuff"), "/tmp/morestuff",
  "And they can set their own copy";

is +Gun->DataFile, "/tmp/stuff", "But it doesn't touch the value on the class";


{
    my $warned = 0;

    local $SIG{__WARN__} = sub {
        if  (shift =~ /DESTROY/i) {
            $warned++;
        };
    };

    Ray->mk_classaccessor('DESTROY');

    ok($warned, 'Warn when creating DESTROY');

    # restore non-accessorized DESTROY
    no warnings;
    *Ray::DESTROY = sub {};
};

eval {
    $obj->mk_classaccessor('foo');
};
like($@, qr{not an object method}, 'Die when used as an object method');

