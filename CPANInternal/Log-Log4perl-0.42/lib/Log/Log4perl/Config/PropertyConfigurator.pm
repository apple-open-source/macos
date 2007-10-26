package Log::Log4perl::Config::PropertyConfigurator;

use strict;

#poor man's export
*eval_if_perl = \&Log::Log4perl::Config::eval_if_perl;
*unlog4j      = \&Log::Log4perl::Config::unlog4j;

use constant _INTERNAL_DEBUG => 0;

################################################
sub parse {
################################################
    my $text = shift;

    my $data = {};
    my %var_subst = ();

    while (@$text) {
        $_ = shift @$text;
        s/^\s*#.*//;
        next unless /\S/;
    
        while (/(.+?)\\\s*$/) {
            my $prev = $1;
            my $next = shift(@$text);
            $next =~ s/^ +//g;  #leading spaces
            $next =~ s/^#.*//;
            $_ = $prev. $next;
            chomp;
        }

        if(my($key, $val) = /(\S+?)\s*=\s*(.*)/) {

            $val =~ s/\s+$//;

                # Everything could potentially be a variable assignment
            $var_subst{$key} = $val;

                # Substitute any variables
            $val =~ s/\${(.*?)}/
                      Log::Log4perl::Config::var_subst($1, \%var_subst)/gex;

            $val = eval_if_perl($val) if 
                $key !~ /\.(cspec\.)|warp_message|filter/;
            $key = unlog4j($key);

            my $how_deep = 0;
            my $ptr = $data;
            for my $part (split /\.|::/, $key) {
                $ptr->{$part} = {} unless exists $ptr->{$part};
                $ptr = $ptr->{$part};
                ++$how_deep;
            }

            #here's where we deal with turning multiple values like this:
            # log4j.appender.jabbender.to = him@a.jabber.server
            # log4j.appender.jabbender.to = her@a.jabber.server
            #into an arrayref like this:
            #to => { value => 
            #       ["him\@a.jabber.server", "her\@a.jabber.server"] },
            if (exists $ptr->{value} && $how_deep > 2) {
                if (ref ($ptr->{value}) ne 'ARRAY') {
                    my $temp = $ptr->{value};
                    $ptr->{value} = [];
                    push (@{$ptr->{value}}, $temp);
                }
                push (@{$ptr->{value}}, $val);
            }else{
                $ptr->{value} = $val;
            }
        }
    }
    return $data;
}

1;

__END__

=head1 NAME

Log::Log4perl::Config::PropertyConfigurator - reads properties file

=head1 SYNOPSIS

This is an internal class.

    Log::Log4perl::Config::PropertyConfigurator::parse($text);

=head1 DESCRIPTION

Initializes log4perl from a properties file, stuff like

    log4j.category.a.b.c.d = WARN, A1
    log4j.category.a.b = INFO, A1

It also understands variable substitution, the following
configuration is equivalent to the previous one:

    settings = WARN, A1
    log4j.category.a.b.c.d = ${settings}
    log4j.category.a.b = INFO, A1

=head1 SEE ALSO

Log::Log4perl::Config

Log::Log4perl::Config::DOMConfigurator

Log::Log4perl::Config::LDAPConfigurator (tbd!)

=head1 AUTHOR

Kevin Goess, <cpan@goess.org> Jan-2003

=cut
