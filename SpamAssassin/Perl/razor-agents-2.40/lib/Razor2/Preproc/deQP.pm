package Razor2::Preproc::deQP;
#use MIME::QuotedPrint;


sub new { 
    return bless {}, shift;
}


sub isit {
    my ($self, $text) = @_;
    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;
    return $hdr =~ /^Content-Transfer-Encoding: quoted-printable/ism;
}


sub doit {

    my ($self, $text) = @_;

    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;

    # comment this out to be compatible with libpreproc.cc:qp_decode()
    #$body =~ s/[ \t]+?(\r?\n)/$1/g;  # rule #3 (trailing space must be deleted)
    $body =~ s/=\r?\n//g;            # rule #5 (soft line breaks)
    $body =~ s/=([\da-fA-F]{2})/pack("C", hex($1))/ge;

    $$text = "$hdr\n\n$body";

    return $text;
}


sub extract_qp {
    my ($self, $text) = @_;

    if ($$text =~ /Content-Transfer-Encoding: quoted-printable/sim) {
        $' =~ /\r?\n\r?\n(.*)$/s;
        return $1;
    }
    return undef;
}

1;
