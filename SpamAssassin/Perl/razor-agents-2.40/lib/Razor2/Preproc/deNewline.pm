package Razor2::Preproc::deNewline;
use MIME::QuotedPrint;


sub new { 
    return bless {}, shift;
}


sub isit {
    1;
}


sub doit {

    my ($self, $text) = @_;

    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;

    return unless $body;

    unless ($body =~ s/\n+$//s) { 
        return $text;
    }

    $$text = "$hdr\n\n$body";
    return $text;

}


1;
