##
##  ca-bundle.pl -- Regenerate ca-bundle.crt from the Mozilla certdata.txt
##

#   configuration
my $cvsroot  = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
my $certdata = 'mozilla/security/nss/lib/ckfw/builtins/certdata.txt';

my $date = `date`;
$date =~ s/\n$//s;
print <<EOH;
##
##  ca-bundle.crt -- Bundle of CA Root Certificates
##
##  This is a bundle of X.509 certificates of public Certificate
##  Authorities (CA). These were automatically extracted from Mozilla's
##  root CA list (the file `certdata.txt'). It contains the certificates
##  in both plain text and PEM format and therefore can be directly used
##  with an Apache/mod_ssl webserver for SSL client authentication. Just
##  configure this file as the SSLCACertificateFile.
##
##  (SKIPME)
##
##  Last Modified: $date
EOH
open(IN, "cvs -d $cvsroot co -p $certdata|")
    || die "could not check out certdata.txt";
my $incert = 0;
while (<IN>) {
    if (/^CKA_VALUE MULTILINE_OCTAL/) {
        $incert = 1;
        open(OUT, "|openssl x509 -text -inform DER -fingerprint")
            || die "could not pipe to openssl x509";
    } elsif (/^END/ && $incert) {
        close(OUT);
        $incert = 0;
        print "\n\n";
    } elsif ($incert) {
        my @bs = split(/\\/);
        foreach my $b (@bs) {
            chomp $b;
            printf(OUT "%c", oct($b)) unless $b eq '';
        }
    } elsif (/^CVS_ID.*Revision: ([^ ]*).*/) {
        print "##  Source: \"certdata.txt\" CVS revision $1\n##\n\n";
    }
}
close(IN);

