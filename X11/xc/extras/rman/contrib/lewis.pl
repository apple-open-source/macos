#!/usr/bin/perl

# Syntax: man2html <topic> <section>

# Print out a content-type for HTTP/1.0 compatibility
print "Content-type: text/html\n\n";

if( $ENV{'REQUEST_METHOD'} eq "GET" )
{
   $buffer=$ENV{'QUERY_STRING'} ;
   @ARGV = split(/&/,$buffer) ;
}

@manpage = `man @ARGV[1] @ARGV[0] | rman -f html -n @ARGV[0] -s
@ARGV[1]`;

print "@manpage";
