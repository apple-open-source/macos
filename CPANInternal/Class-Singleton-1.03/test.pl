#
# Class::Singleton test script
#
# Andy Wardley <abw@cre.canon.co.uk>
#

BEGIN { 
    $| = 1; 
    print "1..22\n"; 
}

END   { 
    print "not ok 1\n" unless $loaded;
}

use Class::Singleton;

$loaded = 1;
print "ok 1\n";

# turn warnings on
$^W = 1;



#========================================================================
#                           -- UTILITY SUBS --
#========================================================================

sub ok     {
    return join('', @_ ? ("   ", @_, "\n") : (), "ok ",     ++$loaded, "\n");
}

sub not_ok { 
    return join('', @_ ? ("   ", @_, "\n") : (), "not ok ", ++$loaded, "\n");
}



#========================================================================
#                         -- CLASS DEFINTIONS --
#========================================================================

#------------------------------------------------------------------------
# define 'DerivedSingleton', a class derived from Class::Singleton 
#------------------------------------------------------------------------

package DerivedSingleton;
use base 'Class::Singleton';


#------------------------------------------------------------------------
# define 'AnotherSingleton', a class derived from DerivedSingleton 
#------------------------------------------------------------------------

package AnotherSingleton;
use base 'DerivedSingleton';


#------------------------------------------------------------------------
# define 'ListSingleton', which uses a list reference as its type
#------------------------------------------------------------------------

package ListSingleton;
use base 'Class::Singleton';

sub _new_instance {
    my $class  = shift;
    bless [], $class;
}


#------------------------------------------------------------------------
# define 'ConfigSingleton', which has specific configuration needs.
#------------------------------------------------------------------------

package ConfigSingleton;
use base 'Class::Singleton';

sub _new_instance {
    my $class  = shift;
    my $config = shift || { };
    my $self = {
	'one' => 'This is the first parameter',
	'two' => 'This is the second parameter',
	%$config,
    };
    bless $self, $class;
}



#========================================================================
#                                -- TESTS --
#========================================================================

package main;

# call Class::Singleton->instance() twice and expect to get the same 
# reference returned on both occasions.

my $s1 = Class::Singleton->instance();

#2 
print "   Class::Singleton instance 1: ",
    defined($s1) ? ok($s1) : not_ok('<undef>');

my $s2 = Class::Singleton->instance();

#3
print "   Class::Singleton instance 2: ",
    (defined($s2) ? ok($s2) : not_ok('<undef>'));

#4
print $s1 == $s2 
    ? ok('Class::Singleton instances are identical') 
    : not_ok('Class::Singleton instances are unique');


# call MySingleton->instance() twice and expect to get the same 
# reference returned on both occasions.

my $s3 = DerivedSingleton->instance();

#5
print "   DerivedSingleton instance 1: ", 
    defined($s3) ? ok($s3) : not_ok('<undef>');

my $s4 = DerivedSingleton->instance();

#6
print "   DerivedSingleton instance 2: ", 
    defined($s4) ? ok($s4) : not_ok('<undef>');

#7
print $s3 == $s4 
    ? ok("DerivedSingleton instances are identical")
    : not_ok("DerivedSingleton instances are unique");


# call MyOtherSingleton->instance() twice and expect to get the same 
# reference returned on both occasions.

my $s5 = AnotherSingleton->instance();

#8
print "   AnotherSingleton instance 1: ",
    defined($s5) ? ok($s5) : not_ok('<undef>');

my $s6 = AnotherSingleton->instance();

#9
print "   AnotherSingleton instance 2: ",
    defined($s6) ? ok($s6) : not_ok('<undef>');

#10
print $s5 == $s6 
    ? ok("AnotherSingleton instances are identical")
    : not_ok("AnotherSingleton instances are unique");


#------------------------------------------------------------------------
# having checked that each instance of the same class is the same, we now
# check that the instances of the separate classes are actually different 
# from each other 
#------------------------------------------------------------------------

#11-13
print $s1 != $s3 
    ? ok("Class::Singleton and DerviedSingleton are different") 
    : not_ok("Class::Singleton and DerivedSingleton are identical");
print $s1 != $s5 
    ? ok("Class::Singleton and AnotherSingleton are different") 
    : not_ok("Class::Singleton and AnotherSingleton are identical");
print $s3 != $s5 
    ? ok("DerivedSingleton and AnotherSingleton are different") 
    : not_ok("DerivedSingleton and AnotherSingleton are identical");



#------------------------------------------------------------------------
# test ListSingleton
#------------------------------------------------------------------------

my $ls1 = ListSingleton->instance();
my $ls2 = ListSingleton->instance();

#14
print $ls1
    ? ok("ListSingleton #1 is defined")
    : not_ok("ListSingleton #1 is not defined");

#15
print $ls2
    ? ok("ListSingleton #2 is defined")
    : not_ok("ListSingleton #2 is not defined");

#16 - check they are the same reference
print $ls1 == $ls2
    ? ok("ListSingleton #1 and #2 correctly reference the same list")
    : not_ok("ListSingleton #1 and #2 so not reference the same list");

#17 - check it's a LIST reference
print $ls1 =~ /=ARRAY/
    ? ok("ListSingleton correctly contains a list reference")
    : not_ok("ListSingleton does not contain a list reference");



#------------------------------------------------------------------------
# test ConfigSingleton
#------------------------------------------------------------------------

# create a ConfigSingleton
my $config = { 'foo' => 'This is foo' };
my $cs1 = ConfigSingleton->instance($config);

# add another parameter to the config
$config->{'bar'} => 'This is bar';

# shouldn't call new() so changes to $config shouldn't matter
my $cs2 = ConfigSingleton->instance($config);

#18
print $cs1
    ? ok("ConfigSingleton #1 is defined")
    : not_ok("ConfigSingleton #1 is not defined");

#19
print $cs2
    ? ok("ConfigSingleton #2 is defined")
    : not_ok("ConfigSingleton #2 is not defined");

#20 - check they are the same reference
print $cs1 == $cs2
    ? ok("ConfigSingleton #1 and #2 correctly reference the same object")
    : not_ok("ConfigSingleton #1 and #2 so not reference the same object");

#21 - check that 3 keys are defined in $cs1
print scalar(keys %$cs1) == 3
    ? ok("ConfigSingleton #1 correctly has 3 keys")
    : not_ok("ConfigSingleton #1 does not have 3 keys");

#22 - and also in $cs2
print scalar(keys %$cs2) == 3
    ? ok("ConfigSingleton #2 correctly has 3 keys")
    : not_ok("ConfigSingleton #2 does not have 3 keys");





