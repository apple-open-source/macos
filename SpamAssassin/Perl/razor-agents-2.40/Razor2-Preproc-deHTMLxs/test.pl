# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use Razor2::Preproc::deHTMLxs;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):


my $dh = new Razor2::Preproc::deHTMLxs;

print "NOT " unless $dh->is_xs;
print "ok 2\n";

my $debug=0;

my $hdr = "X-Razor2-Dummy: foo\n\n";

my $testnum = 3;
foreach my $html_fn (qw(
    html.1 html.2 html.3
    )) {
    my $fn = "testit/$html_fn";
    open(IN, "$fn") or die "cant read $fn";
    my $html = $hdr . join '', <IN>;
    close IN;
    
    if ($dh->isit(\$html)) {


        #my $cleaned_ref = $dh->doit(\$cleaned);
        #my $cleaned = $$cleaned_ref;

        my $cleaned = $html;
        $dh->doit(\$cleaned);

        $cleaned =~ s/^$hdr//s;

        print "html: $fn (len=". length($html) .") cleaned len=". length($cleaned) ."\n" if $debug;
        #print "NOT " unless $cleaned eq $dehtml;

        if ( $^O eq 'VMS' ) {open(IN, "${fn}_deHTMLxs") or die "cant read ${fn}_deHTMLxs";}
        if ( $^O ne 'VMS' ) {open(IN, "$fn.deHTMLxs") or die "cant read $fn.deHTMLxs";}
 
        my $dehtml = join '', <IN>;
        close IN;

        if ($cleaned eq $dehtml) {
            print " -- YEAH -- cleaned html is same as .deHTMLxs: $fn\n" if $debug;
        } else {
            print "NOT ";
            print "cleaned html (len=". length($cleaned) .") differs from .deHTMLxs (len=". 
                length($dehtml) .")\n" if $debug;
        }
    } else {
        print "not html: $fn (len=". length($html) .")\n" if $debug;
        print "NOT ";
    }
    print "ok ". $testnum++ ."\n";
}
