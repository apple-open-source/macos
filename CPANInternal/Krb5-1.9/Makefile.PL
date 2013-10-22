use ExtUtils::MakeMaker;

##### CHANGE THESE ACCORDING TO YOUR CONFIGURATION #####

# location of Kerberos 5 libraries
my $KRB5_LIBDIR = '/usr/lib';

# any extra libraries?
# add -lresolv here if you get errors like the following (usually on linux):
#  undefined symbol: __res_search
my $KRB5_EXTRALIBS = '-lresolv';

# location of Kerberos 5 includes
my $KRB5_INCDIR = '/usr/include';

# any extra include flags?
my $KRB5_EXTRAINCS = '';

##### DO NOT CHANGE ANYTHING BELOW HERE #####

# check for libk5crypto -- only in krb5-1.1 and above
print "Checking for libk5crypto...";
my $cryptolib;
if ( -r "${KRB5_LIBDIR}/libk5crypto.a" || -r "${KRB5_LIBDIR}/libk5crypto.so" ) {
	print "yes\n";
	$cryptolib = '-lk5crypto';
}
else {
	print "no.  I'll use libcrypto instead.\n";
	$cryptolib = '-lcrypto';
}

WriteMakefile(
    'NAME'	=> 'Authen::Krb5',
    'VERSION_FROM' => 'Krb5.pm',
    'LIBS'	=> ["-L${KRB5_LIBDIR} -lkrb5 ${cryptolib} -lcom_err $KRB5_EXTRALIBS"],
    'DEFINE'	=> '',
    'INC'	=> "-I${KRB5_INCDIR} $KRB5_EXTRAINCS"
);
